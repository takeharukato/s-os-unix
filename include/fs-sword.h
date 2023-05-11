/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator SWORD file system module header                    */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_FS_SWORD_H)
#define _FS_SWORD_H

#include "sim-type.h"

#define FS_SWD_FSNAME     "SWORD"  /**< File system name */
#define FS_SWD_ROOT_VNID  (0)      /**< root v-node ID */

/** Refer the file allocation table array
    @return The address of the file allocation table array
 */
#define FS_SWD_REF_FAT_TBL(_fatp)		\
	(  (fs_sword_fatent *)&( ( (struct _fs_sword_fat *)(_fatp) )->fat[0] ) )

/** Refer the address of the file allocation table entry
    @param[in] _fatp The address of the file allocation table struct (struct _fs_sword_fat).
    @param[in] _idx  The index number of the file allocation table array
    (it's a block number on SWORD).
    @return The address of the file allocation table entry
 */
#define FS_SWD_REF_FAT(_fatp, _idx)		\
	( FS_SWD_REF_FAT_TBL((_fatp)) + (_idx) )

/** Refer the file allocation table entry
    @param[in] _fatp The address of the file allocation table struct (struct _fs_sword_fat).
    @param[in] _idx  The index number of the file allocation table array
    (it's a block number on SWORD).
    @return The value of the file allocation table entry (it means the next cluster number).
 */
#define FS_SWD_GET_FAT(_fatp, _idx)			\
	( *FS_SWD_REF_FAT( (_fatp), (_idx) ) )

/** Set the file allocation table entry
    @param[out] _fatp The address of the file allocation table struct (struct _fs_sword_fat).
    @param[in] _idx  The index number of the file allocation table array
    @param[in] _v    The new value of the file allocation table entry
    (it's a block number on SWORD).
 */
#define FS_SWD_SET_FAT(_fatp, _idx, _v) do{				\
		*FS_SWD_REF_FAT( (_fatp), (_idx) ) = SOS_FAT_VAL((_v));	\
	}while(0)

/** Determine whether the cluster is the cluster at the end of file.
    @param[in] _nxt_cls the next cluster number of the cluster to examine.
    @retval TRUE   The cluster is placed at the end of file.
    @retval FALSE  The cluster is NOT placed at the end of file.
 */
#define FS_SWD_IS_END_CLS(_nxt_cls) ( ( (_nxt_cls) & SOS_FAT_ENT_EOF_MASK ) != 0 )

/** Determine whether the cluster number is valid.
    @param[in] _nxt_cls the next cluster number of the cluster to examine
    @retval TRUE   The cluster is valid.
    @retval FALSE  The cluster is valid.
 */
#define FS_SWD_IS_VALID(_cls) \
	( ( (_cls) >= SOS_RESERVED_FAT_NR ) && ( SOS_MAX_FILE_CLUSTER_NR >= (_cls) ) )

/** Calculate how many records are used in the cluster at the end of the file
    @param[in] _ent The value of the file allocation table entry at the end of the file
    @return The number of used records in the cluster at the end of the file
 */
#define FS_SWD_FAT_END_CLS_RECS(_ent) ( ( (_ent) & 0xf ) + 1 )

/** Calculate the number of records in the last cluster of the cluster chain.
    @param[in] _pos The file position at the end of file.
    @return The number of records in the last cluster (unit: the number of records).
 */
#define FS_SWD_CALC_RECS_AT_LAST_CLS(_pos)				\
	( SOS_CALC_NEXT_ALIGN( (_pos), SOS_RECORD_SIZE) / SOS_RECORD_SIZE )

/** Calculate the FAT entry value of the end of the file at _POS
    @param[in] _pos The file position at the end of file.
    @return The FAT entry value of the end of the cluster chain.
 */
#define FS_SWD_CALC_FAT_ENT_AT_LAST_CLS(_pos)	\
	( SOS_FAT_ENT_EOF_MASK | \
	    ( ( FS_SWD_CALC_RECS_AT_LAST_CLS( (_pos) )  - 1 ) & 0xf ) )

/** Fix size for loop with ssize_t
    @param[in] _siz The size value of the buffer.
    @return   The size value of the buffer or SSIZE_MAX
    if the size of the buffer is longer than SSIZE_MAX.
 */
#define FS_SWD_SIZE_FOR_LOOP(_siz) ( ( (_siz) > SSIZE_MAX ) ? (SSIZE_MAX) : (_siz) )

/** Calculate #DIRNO from the offset position in the directory entry.
    @param[in] _pos The file position in the directory entry.
    @return #DIRNO of the file
 */
#define FS_SWD_OFF2DIRNO(_pos) ( (_pos) / SOS_DENTRY_SIZE )

/** Calculate the offset position in the directory entry from #DIRNO.
    @param[in] _dirno #DIRNO of the file
    @return The offset position in the directory entry
 */
#define FS_SWD_DIRNO2OFF(_dirno) ( (_dirno) * SOS_DENTRY_SIZE )

/** Determine whether the open flags is invalid
    @param[in] _attr The file attribute in the file information block or
    the directory entry.
   @param[in] _f The open flags
   @retval TRUE  The open flags is invalid
   @retval FALSE The open flags is valid
 */
#define FS_SWD_IS_OPEN_FLAGS_INVALID(_attr, _f)				\
        ( ( ( (_f) & FS_VFS_FD_FLAG_MAY_WRITE ) == FS_VFS_FD_FLAG_O_CREAT ) || \
	    !SOS_FATTR_IS_VALID(_attr) )


/** Foward declarations
 */
struct _storage_fib;
struct    _fs_ioctx;

/** Data types
 */

/** File Allocation Table
 */
struct _fs_sword_fat{
	fs_sword_fatent fat[SOS_FAT_SIZE];  /**< File Allocation Table */
};

/** Mount option
 */
struct _fs_sword_mnt_opt{
	uint32_t mount_opts;
};

int fs_swd_get_block_number(const struct _fs_ioctx *ioctx, struct _storage_fib *_fib,
    fs_off_t _offset, int _mode, fs_blk_num *_blkp);
int fs_swd_release_blocks(const struct _fs_ioctx *ioctx, struct _storage_fib *_fib,
    fs_off_t _offset, fs_blk_num *_relblkp);
int fs_swd_get_used_size_in_block(const struct _fs_ioctx *ioctx,
    struct _storage_fib *_fib, fs_off_t _offset, size_t *_usedsizp);

int fs_swd_search_dent_by_dirno(sos_devltr _ch, struct _fs_ioctx *ioctx,
    const struct _fs_vnode *_dir_vnode, fs_dirno _dirno, struct _storage_fib *_fib);
int fs_swd_search_dent_by_name(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    const struct _fs_vnode *_dir_vnode, const BYTE *_swd_name, vfs_vnid *_vnidp);
int fs_swd_search_fib_by_vnid(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    vfs_vnid _vnid, struct _storage_fib *_fib);
int fs_swd_search_free_dent(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    const struct _fs_vnode *_dir_vnode, vfs_vnid *_vnidp);
int fs_swd_write_dent(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    const struct _fs_vnode *_dir_vnode, struct _storage_fib *_fib);
int fs_swd_cmp_directory(const struct _fs_vnode *_v1, const struct _fs_vnode *_v2);

int fs_swd_read_block(const struct _fs_ioctx *ioctx, struct _storage_fib *_fib, fs_off_t _pos, BYTE *_buf, size_t _bufsiz, size_t *_rwsizp);
int fs_swd_write_block(const struct _fs_ioctx *ioctx, struct _storage_fib *_fib, fs_off_t _pos, const BYTE *_buf, size_t _bufsiz, size_t *_rwsizp);

int fs_sword2unix(const BYTE *_swordname, char **_destp);
int fs_unix2sword(const char *_unixname, BYTE *_dest, size_t _size);
int fs_compare_unix_and_sword(const char *_unixname, const BYTE *_sword, size_t _len);
void fs_get_sos_header(const struct _storage_fib *_fib, void *_dest, size_t _bufsiz);
int fs_show_dir(sos_devltr _ch, struct _fs_ioctx *_ioctx, const char *_path);

int fops_mount_sword(sos_devltr _ch, const void *_args,
    struct _fs_ioctx *_ioctx, vfs_fs_super *_superp,
    vfs_mnt_flags *_mnt_flagsp, struct _fs_vnode **_root_vnodep);
int fops_unmount_sword(sos_devltr _ch, vfs_fs_super _super,
    struct _fs_vnode *_root_vnode);
int fops_get_vnode_sword(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    vfs_fs_super _super, vfs_vnid _vnid, struct _fs_vnode *_vnp);
int fops_lookup_sword(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    const struct _fs_vnode *_dir_vnode, const char *_name, vfs_vnid *_vnidp);
int fops_creat_sword(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    struct _fs_vnode *_dir_vn, const char *_name,
    const struct _sword_header_packet *_pkt, vfs_vnid *_new_vnidp, BYTE *_resp);
int fops_unlink_sword(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    struct _fs_vnode *_dir_vn, const char *_name, BYTE *resp);
int fops_open_sword(struct _fs_file_descriptor *_fdp,
    const struct _sword_header_packet *_pkt, fs_fd_flags _flags, BYTE *_resp);
int fops_close_sword(struct _fs_file_descriptor *_fdp, BYTE *_resp);
int fops_read_sword(struct _fs_file_descriptor *_fdp, void *_dest, size_t _count,
    size_t *_rdsizp, BYTE *_resp);
int fops_write_sword(struct _fs_file_descriptor *_fdp, const void *_src,
    size_t _count, size_t *_wrsizp, BYTE *_resp);

int fops_truncate_sword(struct _fs_file_descriptor *_fdp, fs_off_t _offset,
    BYTE *_resp);
int fops_stat_sword(struct _fs_file_descriptor *_fdp, struct _storage_fib *_fib,
    BYTE *_resp);
int fops_seek_sword(struct _fs_file_descriptor *_fdp,
    fs_off_t _offset, int _whence, fs_off_t *_new_posp, BYTE *_resp);
int fops_rename_sword(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    const struct _fs_vnode *_src_vn, const char *_oldname,
    const struct _fs_vnode *_dest_vn, const char *_newname, BYTE *_resp);

int fops_set_attr_sword(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    struct _fs_vnode *_vn, const fs_attr _attr, BYTE *_resp);
int fops_get_attr_sword(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    struct _fs_vnode *_vn, fs_attr *_attrp, BYTE *_resp);

int fops_opendir_sword(struct _fs_dir_stream *_dir, BYTE *_resp);
int fops_readdir_sword(struct _fs_dir_stream *_dir, struct _storage_fib *_fibp,
    BYTE *_resp);
int fops_seekdir_sword(struct _fs_dir_stream *_dir, fs_dirno _dirno, BYTE *_resp);
int fops_telldir_sword(const struct _fs_dir_stream *_dir, fs_dirno *_dirnop,
    BYTE *_resp);
int fops_closedir_sword(struct _fs_dir_stream *_dir, BYTE *_resp);
void init_sword_filesystem(void);
#endif  /*  _FS_SWORD_H  */
