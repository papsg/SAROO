//! Compare a read-only reference with a previously retained source/Git snapshot.
use super::{fail, sha, vandal, Result};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    fs,
    io::Read,
    os::unix::ffi::OsStrExt,
    path::{Component, Path, PathBuf},
    process::Command,
    time::{Duration, Instant},
};

fn limited(path: &Path, maximum: u64) -> Result<Vec<u8>> {
    let mut bytes = Vec::new();
    fs::File::open(path)?
        .take(maximum + 1)
        .read_to_end(&mut bytes)?;
    if bytes.len() as u64 > maximum {
        return fail("Preservation input exceeds its size bound");
    }
    Ok(bytes)
}

fn relative_directory(base: &Path, bytes: &[u8]) -> Result<PathBuf> {
    let name = std::str::from_utf8(bytes)?.trim();
    if name.is_empty() {
        return fail("Empty Git-directory pointer");
    }
    Ok(base.join(name).canonicalize()?)
}

fn git_directories(repo: &Path) -> Result<(PathBuf, PathBuf)> {
    let entry = repo.join(".git");
    let directory = if entry.is_dir() {
        entry.canonicalize()?
    } else {
        let data = limited(&entry, 4096)?;
        relative_directory(
            repo,
            data.strip_prefix(b"gitdir: ")
                .ok_or("Invalid Git pointer")?,
        )?
    };
    let common = if directory.join("commondir").exists() {
        relative_directory(&directory, &limited(&directory.join("commondir"), 4096)?)?
    } else {
        directory.clone()
    };
    Ok((directory, common))
}

fn output_directory(repo: &Path, git: &Path, common: &Path, out: &Path) -> Result<PathBuf> {
    let name = out
        .file_name()
        .ok_or("Output needs a final directory name")?;
    let parent = out
        .parent()
        .ok_or("Output needs an existing parent")?
        .canonicalize()?;
    let path = parent.join(name);
    let references = [
        repo.canonicalize()?,
        git.canonicalize()?,
        common.canonicalize()?,
    ];
    if references
        .iter()
        .any(|reference| path.starts_with(reference))
    {
        return fail("Preservation output must be outside the reference and its Git directories");
    }
    fs::create_dir(&path)?; // Refuse reuse, including a dangling output symlink.
    Ok(path)
}

fn git_output(
    repo: &Path,
    out: &Path,
    label: &str,
    args: &[&str],
    deadline: Instant,
) -> Result<String> {
    let (success, stdout, _) = vandal::execute(
        Command::new("git")
            .args([
                "--no-optional-locks",
                "-c",
                "core.fsmonitor=false",
                "-c",
                "core.untrackedCache=false",
                "-C",
            ])
            .arg(repo)
            .args(args),
        out,
        label,
        deadline,
    )?;
    if !success {
        return fail("Read-only Git observation failed; inspect preserved logs");
    }
    Ok(stdout)
}

fn metadata(
    repo: &Path,
    git: &Path,
    out: &Path,
    phase: &str,
    deadline: Instant,
) -> Result<BTreeMap<String, Value>> {
    let mut state = BTreeMap::new();
    for (name, args) in [
        ("branch", vec!["branch", "--show-current"]),
        ("head", vec!["rev-parse", "HEAD"]),
        ("refs", vec!["show-ref"]),
        ("stashes", vec!["stash", "list", "--format=%H %gd %gs"]),
        (
            "status",
            vec!["status", "--porcelain=v1", "--untracked-files=all"],
        ),
        ("worktrees", vec!["worktree", "list", "--porcelain"]),
    ] {
        state.insert(
            name.into(),
            json!(git_output(
                repo,
                out,
                &format!("{phase}-{name}"),
                &args,
                deadline
            )?),
        );
    }
    state.insert(
        "index_sha256".into(),
        json!(sha(&limited(&git.join("index"), 64 * 1024 * 1024)?)),
    );
    Ok(state)
}

fn tracked(repo: &Path, out: &Path, deadline: Instant) -> Result<BTreeMap<String, String>> {
    let names = git_output(repo, out, "tracked", &["ls-files", "-z"], deadline)?;
    let mut hashes = BTreeMap::new();
    let mut total = 0u64;
    for name in names.split_terminator('\0') {
        if hashes.len() >= 100_000
            || name.is_empty()
            || Path::new(name)
                .components()
                .any(|part| !matches!(part, Component::Normal(_)))
        {
            return fail("Invalid or excessive tracked paths");
        }
        let path = repo.join(name);
        if !path
            .parent()
            .ok_or("Missing tracked parent")?
            .canonicalize()?
            .starts_with(repo)
        {
            return fail("Tracked parent escapes the reference");
        }
        let info = fs::symlink_metadata(&path)?;
        let digest = if info.file_type().is_symlink() {
            sha(fs::read_link(&path)?.as_os_str().as_bytes())
        } else if info.is_file() {
            if info.len() > 512 * 1024 * 1024 {
                return fail("Tracked file exceeds the preservation size bound");
            }
            let mut hash = Sha256::new();
            let mut file = fs::File::open(&path)?;
            let mut block = [0; 65536];
            let mut file_bytes = 0u64;
            loop {
                let count = file.read(&mut block)?;
                if count == 0 {
                    break;
                }
                total += count as u64;
                file_bytes += count as u64;
                if file_bytes > 512 * 1024 * 1024
                    || total > 8 * 1024 * 1024 * 1024
                    || Instant::now() >= deadline
                {
                    return fail("Preservation traversal exceeded its byte/time bound");
                }
                hash.update(&block[..count]);
            }
            format!("{:x}", hash.finalize())
        } else {
            return fail("Tracked entry must be a regular file or symlink");
        };
        if hashes.insert(name.into(), digest).is_some() {
            return fail("Duplicate tracked path");
        }
    }
    Ok(hashes)
}

pub(super) fn run(repo: &Path, baseline: &Path, out: &Path) -> Result<()> {
    let repo = repo.canonicalize()?;
    let (git, common) = git_directories(&repo)?;
    let baseline_bytes = limited(baseline, 32 * 1024 * 1024)?;
    let expected: Value = serde_json::from_slice(&baseline_bytes)?;
    let expected = expected.get("observed").unwrap_or(&expected);
    let out = output_directory(&repo, &git, &common, out)?;
    let deadline = Instant::now() + Duration::from_secs(120);
    let before = metadata(&repo, &git, &out, "before", deadline)?;
    let files = tracked(&repo, &out, deadline)?;
    let after = metadata(&repo, &git, &out, "after", deadline)?;
    let mut observed = json!(after);
    observed["tracked_sha256"] = json!(files);
    let stable = before == after;
    let matches = stable && &observed == expected;
    let comparisons = observed
        .as_object()
        .ok_or("Invalid state")?
        .iter()
        .map(|(k, v)| (k.clone(), expected.get(k) == Some(v)))
        .collect::<BTreeMap<_, _>>();
    fs::write(
        out.join("receipt.json"),
        serde_json::to_vec_pretty(&json!({
            "baseline_sha256":sha(&baseline_bytes), "matches":matches,
            "metadata_stable_during_read":stable, "checks":comparisons,
            "tracked_files":files.len(), "observed":observed,
            "scope":"Sequential tracked-byte and Git-state comparison; not an atomic concurrent-source snapshot. No reference writes or optional locks."
        }))?,
    )?;
    if !matches {
        return fail("Preservation mismatch; inspect the retained receipt");
    }
    println!(
        "Preserved {} tracked paths and complete Git state",
        files.len()
    );
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::unix::fs::symlink;

    #[test]
    fn actual_git_and_same_size_file_or_symlink_changes_cannot_pass() {
        let stamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let base =
            std::env::temp_dir().join(format!("preserve-state-{}-{stamp}", std::process::id()));
        fs::create_dir(&base).unwrap();
        let repo = base.join("reference");
        fs::create_dir(&repo).unwrap();
        let setup = |args: &[&str]| {
            let result = Command::new("git")
                .args([
                    "-c",
                    "core.hooksPath=/dev/null",
                    "-c",
                    "commit.gpgsign=false",
                ])
                .arg("-C")
                .arg(&repo)
                .args(args)
                .output()
                .unwrap();
            assert!(
                result.status.success(),
                "{}",
                String::from_utf8_lossy(&result.stderr)
            );
        };
        setup(&["init", "--initial-branch=main"]);
        fs::write(repo.join("a"), b"one").unwrap();
        symlink("a", repo.join("link")).unwrap();
        setup(&["add", "."]);
        setup(&[
            "-c",
            "user.name=Fixture",
            "-c",
            "user.email=fixture@example.invalid",
            "commit",
            "-m",
            "fixture",
        ]);
        let repo = repo.canonicalize().unwrap();
        let (git, common) = git_directories(&repo).unwrap();
        let initial = output_directory(&repo, &git, &common, &base.join("initial")).unwrap();
        let deadline = Instant::now() + Duration::from_secs(120);
        let mut expected = json!(metadata(&repo, &git, &initial, "initial", deadline).unwrap());
        expected["tracked_sha256"] = json!({"a":sha(b"one"),"link":sha(b"a")});
        let baseline = base.join("baseline.json");
        fs::write(&baseline, serde_json::to_vec(&expected).unwrap()).unwrap();
        run(&repo, &baseline, &base.join("matching")).unwrap();
        fs::write(repo.join("a"), b"two").unwrap();
        assert!(run(&repo, &baseline, &base.join("changed-byte")).is_err());
        let result: Value =
            serde_json::from_slice(&fs::read(base.join("changed-byte/receipt.json")).unwrap())
                .unwrap();
        assert_eq!(result["checks"]["tracked_sha256"], false);
        fs::write(repo.join("a"), b"one").unwrap();
        fs::remove_file(repo.join("link")).unwrap();
        symlink("b", repo.join("link")).unwrap();
        assert!(run(&repo, &baseline, &base.join("changed-link")).is_err());
        setup(&["update-ref", "refs/heads/another", "HEAD"]);
        assert!(run(&repo, &baseline, &base.join("changed-ref")).is_err());
        let result: Value =
            serde_json::from_slice(&fs::read(base.join("changed-ref/receipt.json")).unwrap())
                .unwrap();
        assert_eq!(result["checks"]["refs"], false);
        fs::remove_dir_all(base).unwrap();
    }

    #[test]
    fn output_cannot_write_through_a_reference_or_git_alias_or_replace_evidence() {
        let base = std::env::temp_dir().join(format!("preserve-output-{}", std::process::id()));
        fs::create_dir(&base).unwrap();
        let repo = base.join("reference");
        fs::create_dir(&repo).unwrap();
        let git = repo.join(".git");
        fs::create_dir(&git).unwrap();
        let external = base.join("common");
        fs::create_dir(&external).unwrap();
        symlink(&repo, base.join("alias")).unwrap();
        for path in [
            repo.join("new"),
            git.join("new"),
            external.join("new"),
            base.join("alias/new"),
        ] {
            assert!(output_directory(&repo, &git, &external, &path).is_err());
            assert!(!path.exists());
        }
        let accepted = output_directory(&repo, &git, &external, &base.join("evidence")).unwrap();
        fs::write(accepted.join("sentinel"), b"keep").unwrap();
        assert!(output_directory(&repo, &git, &external, &accepted).is_err());
        assert_eq!(fs::read(accepted.join("sentinel")).unwrap(), b"keep");
        fs::remove_dir_all(base).unwrap();
    }
}
