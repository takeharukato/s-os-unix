/*
   SWORD Emurator main header
*/

#ifndef	_SOS_H_
#define	_SOS_H_

#if defined(__STDC__)
#include <limits.h>
#endif  /*  __STDC__  */

/*
 * Sword control codes
 */
#define SCR_SOS_NUL     (0x0)   /* NUL code on S-OS */
#define SCR_SOS_CLS     (0x0c)  /* CLS code on S-OS */
#define SCR_SOS_CR      (0x0d)  /* CR code on S-OS */
#define SCR_SOS_BREAK   (0x1b)  /* break key code on S-OS */
#define SCR_SOS_RIGHT   (0x1c)  /* right cursor code on S-OS */
#define SCR_SOS_LEFT    (0x1d)  /* left cursor code on S-OS */
#define SCR_SOS_UP      (0x1e)  /* up cursor code on S-OS */
#define SCR_SOS_DOWN    (0x1f)  /* down cursor code on S-OS */

/*
 * Error codes
 */
#define SOS_ERROR_SUCCESS     (0x0)  /* success */
#define SOS_ERROR_IO          (0x1)  /* Device I/O Error */
#define SOS_ERROR_OFFLINE     (0x2)  /* Device Offline */
#define SOS_ERROR_BADF        (0x3)  /* Bad File Descriptor */
#define SOS_ERROR_RDONLY      (0x4)  /* Write Protected */
#define SOS_ERROR_BADR        (0x5)  /* Bad Record */
#define SOS_ERROR_FMODE       (0x6)  /* Bad File Mode */
#define SOS_ERROR_BADFAT      (0x7)  /* Bad Allocation Table */
#define SOS_ERROR_NOENT       (0x8)  /* File not Found */
#define SOS_ERROR_NOSPC       (0x9)  /* Device Full */
#define SOS_ERROR_EXIST       (0xa)  /* File Already Exists */
#define SOS_ERROR_RESERVED    (0xb)  /* Reserved Feature */
#define SOS_ERROR_NOTOPEN     (0xc)  /* File not Open */
#define SOS_ERROR_SYNTAX      (0xd)  /* Syntax Error */
#define SOS_ERROR_INVAL       (0xe)  /* Bad Data */
#define SOS_ERROR_NR          (0xf)  /* The number of error numbers */

/*
 * File attributes
 */
#define SOS_FATTR_FREE    (0x0)   /* Free entry */
#define SOS_FATTR_BIN     (0x1)   /* Binary */
#define SOS_FATTR_BAS     (0x2)   /* Basic  */
#define SOS_FATTR_ASC     (0x4)   /* Ascii  */
#define SOS_FATTR_RSV     (0x8)   /* Reserved */
#define SOS_FATTR_HIDDEN  (0x10)  /* Hidden file */
#define SOS_FATTR_RAW     (0x20)  /* Read after write (RAW) */
#define SOS_FATTR_RONLY   (0x40)  /* Read only */
#define SOS_FATTR_DIR     (0x80)  /* Sub directory */
#define SOS_FATTR_EODENT  (0xFF)  /* End of directory entry */
#define SOS_FATTR_MASK    (0x87)  /* File type mask and clear Hidden/RAW/ReadOnly bits */
/*
   S-OS IOCS call in Z80 memory
*/
#define	SOS_COLD	        (0x1ffd)
#define	SOS_HOT		        (0x1ffa)
#define SOS_VER                 (0x1ff7)
#define SOS_PRINT               (0x1ff4)
#define SOS_PRINTS              (0x1ff1)
#define SOS_LTNL                (0x1ffe)
#define SOS_NL                  (0x1feb)
#define SOS_MSG                 (0x1fe8)
#define SOS_MSX                 (0x1fe5)
#define SOS_MPRNT               (0x1ff2)
#define SOS_TAB                 (0x1fdf)
#define SOS_LPRNT               (0x1fdc)
#define SOS_LPTON               (0x1fd9)
#define SOS_LPTOF               (0x1fd6)
#define SOS_GETL                (0x1fd3)
#define SOS_GETKY               (0x1fd0)
#define SOS_BRKEY               (0x1fcd)
#define SOS_INKEY               (0x1fca)
#define SOS_PAUSE               (0x1fc7)
#define SOS_BELL                (0x1fc4)
#define SOS_PRTHX               (0x1fc1)
#define SOS_PRTHL               (0x1fbe)
#define SOS_ASC                 (0x1fbb)
#define SOS_HEX                 (0x1fb8)
#define SOS_2HEX                (0x1fb5)
#define SOS_HLHEX               (0x1fb2)
#define SOS_WOPEN               (0x1faf)
#define SOS_WRD                 (0x1fac)
#define SOS_FCB                 (0x1fa9)
#define SOS_RDD                 (0x1fa6)
#define SOS_FILE                (0x1fa3)
#define SOS_FSAME               (0x1fa0)
#define SOS_FPRNT               (0x1f9d)
#define SOS_POKE                (0x1f9a)
#define SOS_POKEAT              (0x1f97)
#define SOS_PEEK                (0x1f94)
#define SOS_PEEKAT              (0x1f91)
#define SOS_MON                 (0x1f8e)
#define SOS_HL                  (0x1f81)
#define SOS_GETPC               (0x1f80)
#define SOS_DRDSB               (0x2000)
#define SOS_DWTSB               (0x2003)
#define SOS_DIR                 (0x2006)
#define SOS_ROPEN               (0x2009)
#define SOS_SET                 (0x200c)
#define SOS_RESET               (0x200f)
#define SOS_NAME                (0x2012)
#define SOS_KILL                (0x2015)
#define SOS_CSR                 (0x2018)
#define SOS_SCRN                (0x201b)
#define SOS_LOC                 (0x201e)
#define SOS_FLGET               (0x2021)
#define SOS_RDVSW               (0x2024)
#define SOS_SDVSW               (0x2027)
#define SOS_INP                 (0x202a)
#define SOS_OUT                 (0x202d)
#define SOS_WIDCH               (0x2030)
#define SOS_ERROR               (0x2033)

#define	SOS_BOOT	        (0x2036)

/*
 * S-OS workarea in Z80 memory
 */
#define	SOS_USR                 (0x1f7e)
#define	SOS_DVSW                (0x1f7d)
#define	SOS_LPSW                (0x1f7c)
#define	SOS_PRCNT               (0x1f7a)
#define	SOS_XYADR               (0x1f78)
#define	SOS_KBFAD               (0x1f76)
#define	SOS_IBFAD               (0x1f74)
#define	SOS_SIZE                (0x1f72)
#define	SOS_DTADR               (0x1f70)
#define	SOS_EXADR               (0x1f6e)
#define	SOS_STKAD               (0x1f6c)
#define	SOS_MEMEX               (0x1f6a)
#define	SOS_WKSIZ               (0x1f68)
#define	SOS_DIRNO               (0x1f67)
#define	SOS_MXTRK               (0x1f66)
#define	SOS_DTBUF               (0x1f64)
#define	SOS_FATBF               (0x1f62)
#define	SOS_DIRPS               (0x1f60)
#define	SOS_FATPOS              (0x1f5e)
#define	SOS_DSK                 (0x1f5d)
#define	SOS_WIDTH               (0x1f5c)
#define	SOS_MAXLIN              (0x1f5b)

/*
 * File name length
 */
#define SOS_DRIVE_LETTER_LEN    (2)
#define SOS_FNAME_NAMELEN	(13)
#define	SOS_FNAME_EXTLEN	(3)
#define	SOS_FNAME_LEN	        ( SOS_FNAME_NAMELEN + SOS_FNAME_EXTLEN )
#define SOS_NAMEBF_LEN          \
	( SOS_DRIVE_LETTER_LEN + SOS_FNAME_NAMELEN + SOS_FNAME_EXTLEN )
#define	SOS_MAXIMAGEDRIVES	(4)

/*
 * Disk I/O
 */
#define SOS_RECORD_SIZE         (256) /* Record (Sector) size in byte. */
#define SOS_DENTRY_SIZE         (32)  /* Directory entry size in byte . */
#define SOS_DENTRIES_PER_REC    \
	( SOS_RECORD_SIZE / SOS_DENTRY_SIZE ) /* 8 file entries. */

/*
 * Drive letters
 */
#define SOS_DL_DRIVE_A   'A'
#define SOS_DL_DRIVE_B   'B'
#define SOS_DL_DRIVE_C   'C'
#define SOS_DL_DRIVE_D   'D'
#define SOS_DL_RESV_MIN  'E'
#define SOS_DL_RESV_MAX  'L'
#define SOS_DL_COM_CMT   'T'
#define SOS_DL_MON_CMT   'S'
#define SOS_DL_QD        'Q'

/*
 * SOS File Information Block/Directory Entry offset addresses in EM_IBFAD
 */
#define SOS_FIB_OFF_ATTR  (0)   /**< File Attribute */
#define SOS_FIB_OFF_FNAME (1)   /**< File Name      */
#define SOS_FIB_OFF_SIZE  (18)  /**< File Size      */
#define SOS_FIB_OFF_DTADR (20)  /**< Data Addr      */
#define SOS_FIB_OFF_EXADR (22)  /**< File Size      */

/*
 * Internal workarea
 */
#define SOS_RETPOI              (0x2418)
#define SOS_OPNFG               (0x291e)
#define SOS_FTYPE               (0x291f)
#define SOS_DFDV                (0x2920)

#define	SOS_UNITNO	        (0x2b06)

#define CCP_LINLIM              (2000)
#define SOS_UNIX_BUFSIZ         (2000)
#define TRAP_BUFSIZ             (80)

/*
 * Default Device  Switch (Tape devices)
 */
#define SOS_DVSW_COMMON         (0)
#define SOS_DVSW_MONITOR        (1)
#define SOS_DVSW_QD             (3)

#if defined(PATH_MAX)
#define SOS_UNIX_PATH_MAX       (PATH_MAX)
#else
#define SOS_UNIX_PATH_MAX       (1024)
#endif /* PATH_MAX */

/*
   Emulator setting
*/
#define	EM_STKAD	(0x10f0)   /* Stack grows to lower address. */
#define	EM_IBFAD	(EM_STKAD)
#define	EM_ATTR		(EM_IBFAD + SOS_FIB_OFF_ATTR)  /* 0x10f0 */
#define	EM_FNAME	(EM_IBFAD + SOS_FIB_OFF_FNAME) /* 0x10f1 */
#define	EM_SIZE		(EM_IBFAD + SOS_FIB_OFF_SIZE)  /* 0x1102 */
#define	EM_DTADR	(EM_IBFAD + SOS_FIB_OFF_DTADR) /* 0x1104 */
#define	EM_EXADR	(EM_IBFAD + SOS_FIB_OFF_EXADR) /* 0x1106 */
#define	EM_XYADR	(0x1171)                       /* 0x1171-0x1172 */
#define EM_NAMEBF       (0x1173)  /* 0x1173-0x1184 (SOS_NAMEBF_LEN bytes) */
#define	EM_KBFAD	(0x11a3)
#define	EM_MEMAX	(0xffff)
#define	EM_WKSIZ	(0xffff)
#define	EM_MXTRK	(0x50)
#define	EM_DTBUF	(0x2f00)
#define	EM_FATBF	(0x2e00)
#define	EM_DIRPS	(0x10)
#define	EM_FATPOS	(0x0e)
#define	EM_WIDTH	(0x50)
#define	EM_MAXLN	(25)

#define	EM_DFDV		(SOS_DL_QD)

#define	EM_VER		(0x1620)	/* XXX: SWORD version */

#endif
