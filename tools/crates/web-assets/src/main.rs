//! Deterministic host linker for the project's authored offline browser modules.
//! This accepts a closed module convention, not arbitrary JavaScript syntax.
use regex::{Captures, Regex};
use serde_json::json;
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

type Result<T> = std::result::Result<T, String>;
const INPUT_LIMIT: usize = 1024 * 1024;
const OUTPUT_LIMIT: usize = 2 * 1024 * 1024;
const SAX_SHA: &str = "1cdc1270d987cb0f6ee66dd8efb01c50511aa14d17b6163bbbffb21df8cf1c06";
const SAX_LICENSE_SHA: &str = "8a1af140fdfbf5afd3df27f7e662f989c5b963a300020dfafce42033cae9e004";
const HELP: &str = "web-assets: build a self-contained offline browser editor\n\
Usage: web-assets save APP_DIRECTORY OUTPUT.html\n\
Reads src/, vendor/sax/ and LICENSE inside APP_DIRECTORY.\n\
No package install, network, device access or runtime module loader.\n\
Stdout is a JSON receipt. Outputs use atomic rename and cannot alias an input.\n\
Supports only the authored named-import/declaration-export module convention.\n";

fn sha256(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}

struct Inputs {
    root: PathBuf,
    hashes: BTreeMap<String, String>,
    paths: Vec<PathBuf>,
}

impl Inputs {
    fn read(&mut self, name: &str) -> Result<String> {
        let path = self.root.join(name);
        let mut bytes = Vec::new();
        File::open(&path)
            .map_err(|e| format!("{}: {e}", path.display()))?
            .take((INPUT_LIMIT + 1) as u64)
            .read_to_end(&mut bytes)
            .map_err(|e| e.to_string())?;
        if bytes.len() > INPUT_LIMIT {
            return Err(format!("{name} exceeds the 1 MiB source limit"));
        }
        self.hashes.insert(name.into(), sha256(&bytes));
        self.paths.push(path);
        String::from_utf8(bytes).map_err(|e| e.to_string())
    }
}

fn link_module(source: &str, namespaces: &BTreeMap<&str, &str>) -> Result<String> {
    let imports = Regex::new(r"(?m)^import\s+\{([^}]+)\}\s+from\s+'\./([^']+)';[ \t]*$").unwrap();
    let identifiers =
        Regex::new(r"^\s*[A-Za-z_$][A-Za-z0-9_$]*(?:\s*,\s*[A-Za-z_$][A-Za-z0-9_$]*)*\s*$")
            .unwrap();
    let mut import_code = Vec::new();
    for import in imports.captures_iter(source) {
        let dependency = namespaces
            .get(&import[2])
            .ok_or_else(|| format!("unknown/late import: {}", &import[2]))?;
        if !identifiers.is_match(&import[1]) {
            return Err("unsupported named import".into());
        }
        import_code.push(format!("const {{{}}} = {dependency};", &import[1]));
    }
    let source = imports.replace_all(source, "");
    let declarations = Regex::new(
        r"(?m)^export (?:async )?(const|function|class) ([A-Za-z_$][A-Za-z0-9_$]*)([^\n]*)",
    )
    .unwrap();
    let extra_constants = Regex::new(r",\s*([A-Za-z_$][A-Za-z0-9_$]*)\s*=").unwrap();
    let mut names = Vec::new();
    for declaration in declarations.captures_iter(&source) {
        names.push(declaration[2].to_owned());
        if &declaration[1] == "const" {
            names.extend(
                extra_constants
                    .captures_iter(&declaration[3])
                    .map(|c| c[1].to_owned()),
            );
        }
    }
    if names.iter().collect::<BTreeSet<_>>().len() != names.len() {
        return Err("duplicate declaration export".into());
    }
    let strip = Regex::new(r"(?m)^export ((?:async )?(?:const|function|class) )").unwrap();
    let source = strip.replace_all(&source, |c: &Captures| c[1].to_owned());
    if Regex::new(r"(?m)^\s*(?:import|export)\b")
        .unwrap()
        .is_match(&source)
    {
        return Err("unsupported module syntax".into());
    }
    Ok(format!(
        "(() => {{\n{}\n{source}\nreturn {{{}}};\n}})()",
        import_code.join("\n"),
        names.join(", ")
    ))
}

fn replace_once(template: &mut String, marker: &str, replacement: &str) -> Result<()> {
    if template.matches(marker).count() != 1 {
        return Err(format!("expected exactly one {marker} marker"));
    }
    *template = template.replacen(marker, replacement, 1);
    Ok(())
}

fn html_escape(text: &str) -> String {
    text.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
        .replace('\'', "&#x27;")
}

fn save_editor(inputs: &mut Inputs) -> Result<String> {
    let sax = inputs.read("vendor/sax/lib/sax.js")?;
    let sax_license = inputs.read("vendor/sax/LICENSE.md")?;
    if sha256(sax.as_bytes()) != SAX_SHA || sha256(sax_license.as_bytes()) != SAX_LICENSE_SHA {
        return Err("pinned SAX source/license identity mismatch".into());
    }
    let mut parts = vec![format!(
        "const SAX = (() => {{ const exports = {{}};\n{sax}\nreturn exports; }})();"
    )];
    let mut namespaces = BTreeMap::new();
    for (name, namespace) in [
        ("save-fields.mjs", "Fields"),
        ("save-text.mjs", "Text"),
        ("save-xml.mjs", "XML"),
        ("save-codec.mjs", "Archive"),
        ("save-worker.mjs", "WorkerApp"),
    ] {
        parts.push(format!(
            "const {namespace} = {};",
            link_module(&inputs.read(&format!("src/{name}"))?, &namespaces)?
        ));
        namespaces.insert(name, namespace);
    }
    let worker = format!(
        "'use strict';\n{}\nWorkerApp.bootSaveWorker(SAX);",
        parts.join("\n")
    );
    let app = format!("const SaveApp = {};\nSaveApp.bootSaveApp(JSON.parse(document.getElementById('worker-source').textContent));",
        link_module(&inputs.read("src/save-app.mjs")?, &BTreeMap::new())?);
    if app.to_ascii_lowercase().contains("</script") {
        return Err("application contains an inline script closing tag".into());
    }
    let mut html = inputs.read("src/save.html")?;
    let license = inputs.read("LICENSE")?;
    replace_once(
        &mut html,
        "__SAROO_WORKER__",
        &serde_json::to_string(&worker)
            .unwrap()
            .replace('<', "\\u003c"),
    )?;
    replace_once(&mut html, "__SAROO_APP__", &app)?;
    replace_once(
        &mut html,
        "__SAROO_SAX_LICENSE__",
        &html_escape(&sax_license),
    )?;
    replace_once(&mut html, "__SAROO_APP_LICENSE__", &html_escape(&license))?;
    Ok(html)
}

fn write_output(path: &Path, inputs: &[PathBuf], data: &[u8]) -> Result<()> {
    if data.len() > OUTPUT_LIMIT {
        return Err("HTML exceeds the 2 MiB build limit".into());
    }
    if let Ok(destination) = path.canonicalize() {
        for input in inputs {
            if input.canonicalize().map_err(|e| e.to_string())? == destination {
                return Err("output aliases an input".into());
            }
        }
    }
    let mut name = path
        .file_name()
        .ok_or("output must be a file")?
        .to_os_string();
    name.push(format!(".{}.tmp", std::process::id()));
    let temporary = path.with_file_name(name);
    let mut file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&temporary)
        .map_err(|e| e.to_string())?;
    let result = (|| {
        file.write_all(data).map_err(|e| e.to_string())?;
        file.sync_all().map_err(|e| e.to_string())?;
        drop(file);
        fs::rename(&temporary, path).map_err(|e| e.to_string())
    })();
    if result.is_err() {
        let _ = fs::remove_file(&temporary);
    }
    result
}

fn run(args: Vec<String>) -> Result<()> {
    if args.len() == 1 || (args.len() == 2 && matches!(args[1].as_str(), "--help" | "-h")) {
        print!("{HELP}");
        return Ok(());
    }
    if args.len() != 4 || args[1] != "save" {
        return Err(HELP.into());
    }
    let mut inputs = Inputs {
        root: PathBuf::from(&args[2]),
        hashes: BTreeMap::new(),
        paths: Vec::new(),
    };
    let html = save_editor(&mut inputs)?;
    write_output(Path::new(&args[3]), &inputs.paths, html.as_bytes())?;
    println!(
        "{}",
        serde_json::to_string_pretty(&json!({"format":1,"app":"save","inputs":inputs.hashes,
        "output":{"bytes":html.len(),"sha256":sha256(html.as_bytes())}}))
        .unwrap()
    );
    Ok(())
}

fn main() {
    if let Err(error) = run(std::env::args().collect()) {
        eprintln!("web-assets: {error}");
        std::process::exit(1);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn named_imports_and_multiple_declarations() {
        let result = link_module("import { a, b } from './first.mjs';\nexport const x = 1, y = 2;\nexport async function f() {}\nexport class C {}", &BTreeMap::from([("first.mjs", "First")])).unwrap();
        assert!(result.contains("const { a, b } = First;"));
        assert!(result.contains("return {x, y, f, C};"));
        assert!(!result.contains("export "));
    }

    #[test]
    fn unsupported_and_unresolved_module_forms_fail() {
        for source in [
            "import { a } from './late.mjs';",
            "import { a as b } from './a.mjs';",
            "export default 1;",
            "export { a };",
            "export const a = 1;\nexport const a = 2;",
        ] {
            assert!(
                link_module(source, &BTreeMap::from([("a.mjs", "A")])).is_err(),
                "{source}"
            );
        }
    }

    #[test]
    fn raw_text_and_license_escaping_preserve_data() {
        let worker = "</script><script>\"&日本語";
        let encoded = serde_json::to_string(worker)
            .unwrap()
            .replace('<', "\\u003c");
        assert!(!encoded.contains('<'));
        assert_eq!(serde_json::from_str::<String>(&encoded).unwrap(), worker);
        assert_eq!(html_escape("<&>\"'"), "&lt;&amp;&gt;&quot;&#x27;");
        let mut text = "x__MARKER__y".to_string();
        replace_once(&mut text, "__MARKER__", "done").unwrap();
        assert_eq!(text, "xdoney");
        assert!(replace_once(&mut text, "absent", "").is_err());
        assert!(replace_once(&mut "xx".into(), "x", "").is_err());
    }

    #[test]
    fn rejected_output_retains_destination_and_foreign_temporary() {
        let directory =
            std::env::temp_dir().join(format!("saroo-web-assets-{}", std::process::id()));
        fs::create_dir(&directory).unwrap();
        let path = directory.join("output");
        fs::write(&path, b"before").unwrap();
        assert!(write_output(&path, std::slice::from_ref(&path), b"after").is_err());
        assert!(write_output(&path, &[], &vec![0; OUTPUT_LIMIT + 1]).is_err());
        let temporary = directory.join(format!("output.{}.tmp", std::process::id()));
        fs::write(&temporary, b"foreign").unwrap();
        assert!(write_output(&path, &[], b"after").is_err());
        assert_eq!(fs::read(&temporary).unwrap(), b"foreign");
        assert_eq!(fs::read(&path).unwrap(), b"before");
        fs::remove_file(&temporary).unwrap();
        write_output(&path, &[], b"complete").unwrap();
        assert_eq!(fs::read(&path).unwrap(), b"complete");
        fs::remove_dir_all(directory).unwrap();
    }
}
