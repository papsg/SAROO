/* Preserved public SAROO handlers; original repository notices apply. */
int get_sector_data(void)
{
	int pt, spos, snum;
	PARTITION *pp;

	if(cdb.trans_type){
		set_status(STAT_WAIT | cdb.status);
		return 0;
	}

	pt   = cdb.cr3>>8;
	spos = cdb.cr2;
	snum = cdb.cr4;

	pp = &cdb.part[pt];

	if(spos==0xffff){
		spos = pp->numblocks-1;
	}
	if(snum==0xffff){
		snum = pp->numblocks-spos;
	}
	SSLOG(_BUFIO, "get_sector_data: part=%d(%d) spos=%d snum=%d\n", pt, pp->numblocks, spos, snum);
	if(spos<0 || snum<=0 || (spos+snum) > pp->numblocks){
		set_status(STAT_WAIT | cdb.status);
		return 0;
	}

	cdb.trans_type = TRANS_DATA;
	cdb.trans_part_index = pt;
	cdb.trans_block_start = spos;
	cdb.trans_block_end = spos+snum;
	trans_start();

	set_status(STAT_TRNS | cdb.status);
	return HIRQ_DRDY|HIRQ_EHST;
}
int get_del_sector_data(void)
{
	int pt, spos, snum;
	PARTITION *pp;

	if(cdb.trans_type){
		set_status(STAT_WAIT | cdb.status);
		return 0;
	}

	pt   = cdb.cr3>>8;
	spos = cdb.cr2;
	snum = cdb.cr4;
	pp = &cdb.part[pt];

	SSLOG(_BUFIO, "get_del_sector_data: part=%d(%d) spos=%d snum=%d\n", pt, pp->numblocks, spos, snum);
	if(spos==0xffff){
		spos = pp->numblocks-1;
	}
	if(snum==0xffff){
		snum = pp->numblocks-spos;
	}
	SSLOG(_BUFIO, "                   : part=%d(%d) spos=%d snum=%d\n", pt, pp->numblocks, spos, snum);
	if(spos<0 || snum<=0 || (spos+snum) > pp->numblocks){
		// 格兰蒂亚: 传入了snum=0. 似乎应该直接返回.
		set_status(STAT_WAIT | cdb.status);
		return 0;
	}

	cdb.trans_type = TRANS_DATA_DEL;
	cdb.trans_part_index = pt;
	cdb.trans_block_start = spos;
	cdb.trans_block_end = spos+snum;
	trans_start();

	set_status(STAT_TRNS | cdb.status);
	return HIRQ_DRDY|HIRQ_EHST;
}
int change_dir(void)
{
	int selnum, fid;
	FILE_INFO *fi;

	selnum = cdb.cr3>>8;
	fid = ((cdb.cr3&0xff)<<16) | (cdb.cr4);
	SSLOG(_FILEIO, "change_dir: sel=%d fid=%08x root_lba=%08x\n", selnum, fid, cdb.root_lba);

	cdb.dir_filter = selnum;

	if(fid==0xffffff){
		// 切换到根目录
		cdb.cdir_pfid = 0;
		if(cdb.root_lba==0){
			// 需要先读主描述符扇区,以便取得根目录的LBA地址
			cdb.rdir_state = 1;
			cdb.cddev_filter = selnum;
			cdb.pause_request = 0;
			cdb.play_type = PLAYTYPE_DIR;
			cdb.play_fad_start = 16+150;
			cdb.track = fad_to_track(cdb.play_fad_start);
			cdb.play_fad_end = cdb.play_fad_start+1;
			cdb.old_fad = cdb.fad;
			cdb.fad = cdb.play_fad_start;
			_set_filter(selnum, 0x40, cdb.play_fad_start, 1);
			disk_task_wakeup();
		}else{
			// 已经有根目录的LBA地址
			cdb.cdir_lba = cdb.root_lba;
			cdb.cdir_size = cdb.root_size;

			cdb.rdir_state = 2;
			cdb.cddev_filter = selnum;
			cdb.pause_request = 0;
			cdb.play_type = PLAYTYPE_DIR;
			cdb.play_fad_start = cdb.cdir_lba+150;
			cdb.track = fad_to_track(cdb.play_fad_start);
			cdb.play_fad_end = cdb.play_fad_start+cdb.cdir_size;
			cdb.old_fad = cdb.fad;
			cdb.fad = cdb.play_fad_start;
			_set_filter(selnum, 0x40, cdb.play_fad_start, cdb.cdir_size);
			disk_task_wakeup();
		}
	}else{
		// 切换到fid所指的目录
		if(cdb.cdir_lba){
			if(fid<2){
				fi = &cdb.fi[fid];
			}else{
				if(fid<cdb.cdir_pfid || fid>=(cdb.cdir_pfid+cdb.cdir_pfnum)){
					set_status(STAT_REJECT);
					return 0;
				}else{
					fi = &cdb.fi[fid-cdb.cdir_pfid+2];
				}
			}

			cdb.cdir_lba = fi->lba;
			cdb.cdir_size = fi->size/2048;

			cdb.cdir_pfid = 0;
			cdb.rdir_state = 2;
			cdb.cddev_filter = selnum;
			cdb.pause_request = 0;
			cdb.play_type = PLAYTYPE_DIR;
			cdb.play_fad_start = cdb.cdir_lba+150;
			cdb.track = fad_to_track(cdb.play_fad_start);
			cdb.play_fad_end = cdb.play_fad_start+cdb.cdir_size;
			cdb.old_fad = cdb.fad;
			cdb.fad = cdb.play_fad_start;
			_set_filter(selnum, 0x40, cdb.play_fad_start, cdb.cdir_size);
			disk_task_wakeup();
		}else{
			set_status(STAT_REJECT);
			return 0;
		}
	}

	set_report(cdb.status);
	return 0;
}
int read_dir(void)
{
	int selnum, fid;

	selnum = cdb.cr3>>8;
	fid = ((cdb.cr3&0xff)<<16) | (cdb.cr4);
	SSLOG(_FILEIO, "read_dir: sel=%d fid=%08x\n", selnum, fid);

	if(cdb.cdir_lba){
		cdb.cdir_pfid = fid;
		cdb.rdir_state = 2;
		cdb.cddev_filter = selnum;
		cdb.pause_request = 0;
		cdb.play_type = PLAYTYPE_DIR;
		cdb.play_fad_start = cdb.cdir_lba+150;
		cdb.track = fad_to_track(cdb.play_fad_start);
		cdb.play_fad_end = cdb.play_fad_start+cdb.cdir_size;
		cdb.old_fad = cdb.fad;
		cdb.fad = cdb.play_fad_start;
		_set_filter(selnum, 0x40, cdb.play_fad_start, cdb.cdir_size);
		disk_task_wakeup();
	}else{
		set_status(STAT_REJECT);
		return 0;
	}

	set_report(cdb.status);
	return 0;
}
