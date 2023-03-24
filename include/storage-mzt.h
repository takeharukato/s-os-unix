/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator MZT format tape emulation module header            */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_STORAGE_MZT_H)
#define _STORAGE_MZT_H

/** MZF format of MZ emulator on Linux
    see: https://daimonsoft.info/argo/mztapeall.html#mzt
 */
#define STORAGE_DSKIMG_MZF_LINUX_HEADER "mz20\x00\x02\x00\x00"
#define STORAGE_DSKIMG_MZF_LINUX_HEADER_LEN (8)

/** Japanese MZT format attributes
    See: http://mzakd.cool.coocan.jp/starthp/mzt.html
 */
#define STORAGE_MZT_ATTR_BIN       (0x01)  /**< Binary */
#define STORAGE_MZT_ATTR_BAS       (0x02)  /**< Basic */
#define STORAGE_MZT_ATTR_BSD_QD    (0x03)  /**< Basic Sequantial Data on Quick Disk  */
#define STORAGE_MZT_ATTR_ASC       (0x04)  /**< S-OS Asc/Basic Sequantial Data on tape */
#define STORAGE_MZT_ATTR_SBAS      (0x05)  /**< S-Basic */

/*
 * MZT header
 */
#define STORAGE_MZT_NAME_LEN            (17)
#define STORAGE_MZT_NAME_BUFSIZ         (STORAGE_MZT_NAME_LEN + 1) /* fname + end mark */
#define STORAGE_MZT_NAME_TERM           (0x0d)  /**< End mark */
#define STORAGE_MZT_HEADER_SIZE         (0x80)  /**< 128 bytes */
#define STORAGE_MZT_HEADER_OFF_ATTR     (0x00)  /**< Attribute */
#define STORAGE_MZT_HEADER_OFF_FNAME    (0x01)  /**< File name */
#define STORAGE_MZT_HEADER_OFF_SIZE     (0x12)  /**< File size */
#define STORAGE_MZT_HEADER_OFF_DTADR    (0x14)  /**< File save/load address */
#define STORAGE_MZT_HEADER_OFF_EXADR    (0x16)  /**< File execution address */

/** Refer file infomation in the MZT header
    @param[in] _headp The address of the MZT header
    @param[in] _off   The offset address from the top of the MZT header
    @return The address of the information
 */
#define STORAGE_MZT_FIB_REF(_headp, _off)	\
	( (BYTE *)(_headp) + (_off) )

/** Refer the file attribute in the MZT header
    @param[in] _headp The address of the MZT header
    @return The address of the file attribute in the MZT header
 */
#define STORAGE_MZT_FIB_ATTR(_headp)					\
	( (BYTE *)( STORAGE_MZT_FIB_REF(_headp, STORAGE_MZT_HEADER_OFF_ATTR) ) )

/** Refer the file name in the MZT header
    @param[in] _headp The address of the MZT header
    @return The address of the file name in the MZT header
    @remark The file name is terminated with the end mark (0x0D) and are filled
    by 0x00, 0x0D after the end mark.
    The file name might NOT be terminated with the end mark (0x0D) 0x20
    filled by 0x20(space) instead  (e.g., Hu-Basic) .
 */
#define STORAGE_MZT_FIB_FNAME(_headp)					\
	( (BYTE *)( STORAGE_MZT_FIB_REF(_headp, STORAGE_MZT_HEADER_OFF_FNAME) ) )

/** Refer the file size in the MZT header
    @param[in] _headp The address of the MZT header
    @return The address of the file size in the MZT header (little endian)
*/
#define STORAGE_MZT_FIB_SIZE(_headp)					\
	( (WORD *)( STORAGE_MZT_FIB_REF(_headp, STORAGE_MZT_HEADER_OFF_SIZE) ) )

/** Refer the file save/load address in the MZT header
    @param[in] _headp The address of the MZT header
    @return The address of the file save/load address in the MZT header (little endian)
*/
#define STORAGE_MZT_FIB_DTADR(_headp)					\
	( (WORD *)( STORAGE_MZT_FIB_REF(_headp, STORAGE_MZT_HEADER_OFF_DTADR) ) )

/** Refer the file execution address in the MZT header
    @param[in] _headp The address of the MZT header
    @return The address of the file execution address in the MZT header (little endian)
*/
#define STORAGE_MZT_FIB_EXADR(_headp)					\
	( (WORD *)(STORAGE_MZT_FIB_REF(_headp, STORAGE_MZT_HEADER_OFF_EXADR) ) )

/** MZT header
 */
struct _storage_mzt_header{
	BYTE data[STORAGE_MZT_HEADER_SIZE];
};

#endif  /* _STORAGE_MZT_H  */
