bin_PROGRAMS = photorec_nc

smallbase_C = common.c crc.c ext2_common.c fat_common.c log.c misc.c setdate.c
smallbase_H = common.h crc.h ext2_common.h fat_common.h log.h misc.h setdate.h

photorec_nc_base_C = $(smallbase_C) apfs_common.c autoset.c fnctdsk.c hdaccess.c \
    hdcache.c hidden.c hpa_dco.c intrf.c iso.c list_sort.c log_part.c \
    parti386.c partgpt.c parthumax.c partmac.c partnone.c partsun.c \
    partxbox.c partauto.c ntfs_io.c ntfs_utl.c unicode.c

photorec_nc_base_H = $(smallbase_H) alignio.h apfs_common.h autoset.h fnctdsk.h \
    hdaccess.h hdcache.h hidden.h hpa_dco.h guid_cmp.h guid_cpy.h intrf.h \
    iso.h iso9660.h list.h list_sort.h log_part.h types.h parti386.h partgpt.h \
    parthumax.h partmac.h partsun.h partxbox.h partauto.h unicode.h \
    lang.h ntfs_utl.h

fs_C = analyse.c apfs.c bfs.c bsd.c btrfs.c cramfs.c exfat.c ext2.c fat.c \
    fatx.c f2fs.c jfs.c gfs2.c hfs.c hfsp.c hpfs.c luks.c lvm.c md.c netware.c \
    ntfs.c refs.c rfs.c savehdr.c sun.c swap.c sysv.c ufs.c vmfs.c wbfs.c xfs.c zfs.c

fs_H = analyse.h apfs.h bfs.h bsd.h btrfs.h cramfs.h exfat.h ext2.h fat.h \
    fatx.h f2fs.h f2fs_fs.h jfs_superblock.h jfs.h gfs2.h hfs.h hfsp.h hpfs.h \
    hfsp_struct.h luks.h luks_struct.h lvm.h md.h netware.h ntfs.h ntfs_struct.h \
    refs.h rfs.h savehdr.h sun.h swap.h sysv.h ufs.h vmfs.h wbfs.h xfs.h \
    xfs_struct.h zfs.h

file_C = filegen.c \
    file_list.c \
    file_1cd.c file_3dm.c file_3ds.c file_7z.c file_DB.c file_a.c file_ab.c \
    file_abr.c file_acb.c file_ace.c file_ado.c file_afdesign.c file_ahn.c \
    file_aif.c file_all.c file_als.c file_amd.c file_amr.c file_apa.c file_ape.c \
    file_apple.c file_ari.c file_arj.c file_asf.c file_asl.c file_asm.c \
    file_atd.c file_au.c file_axp.c file_axx.c file_bac.c file_bdm.c \
    file_berkeley.c file_bfa.c file_bim.c file_bin.c file_binvox.c file_bkf.c \
    file_bld.c file_bmp.c file_bpg.c file_bvr.c file_bz2.c file_c4d.c file_cab.c \
    file_caf.c file_cam.c file_catdrawing.c file_cdt.c file_che.c file_chm.c \
    file_class.c file_clip.c file_cm.c file_compress.c file_cow.c file_cpi.c \
    file_crw.c file_csh.c file_ctg.c file_cwk.c file_d2s.c file_dad.c file_dar.c \
    file_dat.c file_dbf.c file_dbn.c file_dcm.c file_ddf.c file_dex.c file_dim.c \
    file_dir.c file_djv.c file_dmp.c file_doc.c file_dovecot.c file_dpx.c \
    file_drw.c file_drw2.c file_ds2.c file_ds_store.c file_dsc.c file_dss.c \
    file_dst.c file_dta.c file_dump.c file_dv.c file_dvi.c file_dvr.c file_dwg.c \
    file_dxf.c file_e01.c file_edb.c file_ecryptfs.c file_elf.c file_emf.c \
    file_ess.c file_evt.c file_evtx.c file_exe.c file_exr.c file_exs.c \
    file_ext.c file_ext2.c file_fat.c file_fbf.c file_fbk.c file_fbx.c \
    file_fcp.c file_fcs.c file_fdb.c file_fds.c file_fh10.c file_fh5.c \
    file_filevault.c file_fits.c file_fit.c file_flac.c file_flp.c file_flv.c \
    file_fm.c file_fob.c file_fos.c file_fp5.c file_fp7.c file_freeway.c \
    file_frm.c file_fs.c file_fwd.c file_fxp.c file_gam.c file_gct.c file_gho.c \
    file_gi.c file_gif.c file_gm6.c file_gp2.c file_gp5.c file_gpg.c file_gpx.c \
    file_gsm.c file_gz.c file_hdf.c file_hdf5.c file_hdr.c file_hds.c \
    file_hfsp.c file_hm.c file_hr9.c file_http.c file_ibd.c file_icc.c \
    file_icns.c file_ico.c file_ifo.c file_idx.c file_imb.c file_indd.c \
    file_info.c file_iso.c file_it.c file_itu.c file_jks.c file_journal.c \
    file_jpg.c file_jsonlz4.c file_kdb.c file_kdbx.c file_key.c file_ldf.c \
    file_lit.c file_lnk.c file_logic.c file_lso.c file_luks.c file_lxo.c \
    file_lz.c file_lzh.c file_lzo.c file_m2ts.c file_mat.c file_max.c \
    file_maxis.c file_mb.c file_mcd.c file_mdb.c file_mdf.c file_mdp.c \
    file_mfa.c file_mfg.c file_mft.c file_mid.c file_mig.c file_mk5.c \
    file_mkv.c file_mlv.c file_mobi.c file_mov.c file_mp3.c file_mpg.c \
    file_mpl.c file_mrw.c file_msa.c file_mus.c file_mxf.c file_myo.c \
    file_mysql.c file_nd2.c file_nds.c file_nes.c file_njx.c file_nk2.c \
    file_nsf.c file_oci.c file_ogg.c file_one.c file_orf.c file_pa.c \
    file_paf.c file_pap.c file_par2.c file_pcap.c file_pcb.c file_pct.c \
    file_pcx.c file_pdb.c file_pdf.c file_pds.c file_pf.c file_pfx.c \
    file_pgdump.c file_plist.c file_plr.c file_plt.c file_png.c file_pnm.c \
    file_prc.c file_prd.c file_prt.c file_ps.c file_psb.c file_psd.c file_psf.c \
    file_psp.c file_pst.c file_ptb.c file_ptf.c file_pyc.c file_pzf.c \
    file_pzh.c file_qbb.c file_qdf.c file_qkt.c file_qxd.c file_r3d.c \
    file_ra.c file_raf.c file_rar.c file_raw.c file_rdc.c file_reg.c \
    file_res.c file_rfp.c file_riff.c file_rm.c file_rns.c file_rpm.c \
    file_rvl.c file_rw2.c file_rx2.c file_save.c file_sdsk.c file_sdw.c \
    file_ses.c file_sgcta.c file_shn.c file_shp.c file_sib.c file_sig.c \
    file_sit.c file_skd.c file_skp.c file_snag.c file_sp3.c file_spe.c \
    file_spf.c file_spss.c file_sql.c file_sqm.c file_steuer2014.c file_stl.c \
    file_stu.c file_studio.c file_swf.c file_tar.c file_tax.c file_tg.c \
    file_tib.c file_tiff.c file_tiff_be.c file_tiff_le.c file_tivo.c \
    file_torrent.c file_tph.c file_tpl.c file_ttf.c file_txt.c file_tz.c \
    file_v2i.c file_vault.c file_vdi.c file_vdj.c file_veg.c file_vfb.c \
    file_vib.c file_vmdk.c file_vmg.c file_wad.c file_wallet.c file_wdp.c \
    file_wee.c file_wim.c file_win.c file_wks.c file_wld.c file_wmf.c file_wnk.c \
    file_woff.c file_wpb.c file_wpd.c file_wtv.c file_wv.c file_x3f.c \
    file_x3i.c file_x4a.c file_xar.c file_xcf.c file_xfi.c file_xfs.c \
    file_xm.c file_xml.c file_xsv.c file_xpt.c file_xv.c file_xz.c file_z2d.c \
    file_zcode.c file_zip.c file_zpr.c \
    utfsize.c

file_H = ext2.h hfsp_struct.h filegen.h file_doc.h file_jpg.h file_gz.h \
    file_riff.h file_sp3.h file_tar.h file_tiff.h luks_struct.h ntfs_struct.h \
    ole.h pe.h suspend.h utfsize.h xfs_struct.h file_found.h memmem.h

photorec_nc_C = phncmain.c ptree.c pscanner.c pbrowser.c prestore.c luksnc.c \
    pcarver.c pbackup.c dir.c dirn.c fat_dir.c ntfs_dir.c ext2_dir.c exfat_dir.c \
    rfs_dir.c geometry.c

photorec_nc_H = photorec_nc.h luksnc.h pbackup.h dir.h dirn.h dir_common.h \
    fat_dir.h ntfs_dir.h ntfs_inc.h ext2_dir.h ext2_inc.h \
    exfat_dir.h rfs_dir.h geometry.h autoset.h

photorec_nc_ncurses_C = askloc.c hiddenn.c chgarchn.c geometryn.c \
    intrfn.c nodisk.c pbanner.c

photorec_nc_ncurses_H = askloc.h hiddenn.h chgarchn.h geometryn.h \
    intrfn.h nodisk.h partauto.h

photorec_nc_C_SOURCES = $(photorec_nc_C) $(photorec_nc_ncurses_C) \
    $(photorec_nc_base_C) $(fs_C) $(file_C) \
    chgtype.c partgptw.c ntfs_udl.c next.c suspend_no.c

photorec_nc_H_SOURCES = $(photorec_nc_H) $(photorec_nc_ncurses_H) \
    $(photorec_nc_base_H) $(fs_H) $(file_H) chgtype.h ntfs_udl.h \
    next.h intrf.h suspend.h

photorec_nc_SOURCES = $(photorec_nc_C_SOURCES) $(photorec_nc_H_SOURCES)

photorec_nc_LDADD = $(photorec_LDADD)

DISTCLEANFILES = *~ core
