#ifndef PST_H
#define PST_H
#include <stdio.h>
#include <stdint.h>

#define PST_MAGIC 					0x4e444221

#define PR_IMPORTANCE				0x0017
#define PR_MESSAGE_CLASS        	0x001A
#define PR_SUBJECT              	0x0037
#define PR_CLIENT_SUBMIT_TIME		0x0039
#define PR_SENDER_NAME          	0x0042
#define PR_SENDER               	0x0C1A
#define PR_DISPLAY_CC           	0x0E03
#define PR_DISPLAY_TO           	0x0E04
#define PR_MESSAGE_DELIVERY_TIME	0x0E06
#define PR_MESSAGE_FLAGS        	0x0E07
#define PR_MESSAGE_RECIPIENTS   	0x0E12
#define PR_MESSAGE_ATTACHMENTS		0x0E13
	#define MSGFLAG_READ			0x0001
	#define MSGFLAG_UNMODIFIED		0x0002
	#define MSGFLAG_SUBMIT			0x0004
	#define MSGFLAG_UNSENT			0x0008
	#define MSGFLAG_HASATTACH		0x0010
	#define MSGFLAG_FROMME			0x0020
	#define MSGFLAG_ASSOCIATED		0x0040
	#define MSGFLAG_RESEND			0x0080
	#define MSGFLAG_RN_PENDING		0x0400
	#define MSGFLAG_NRN_PENDING		0x0800

#define PR_HASATTACH            	0x0E1B
#define PR_ATTACH_SIZE          	0x0E20
#define PR_ENTRYID              	0x0FFF
#define PR_BODY                 	0x1000
#define PR_RTF_COMPRESSED      		0x1009
#define PR_HTML						0x1013
#define PR_DISPLAY_NAME         	0x3001
#define PR_ATTACH_LONG_FILENAME 	0x3701
#define PR_PID_TAG_BODY         	0x6619			


#define NID_TYPE_NORMAL_FOLDER		0x02
#define NID_TYPE_NORMAL_MESSAGE 	0x04
#define NID_TYPE_ATTACHMENT			0x05
#define NID_TYPE_RECIPIENT_TABLE 	0x12
#define NID_TYPE_NULL				0xFF

//https://learn.microsoft.com/en-us/openspecs/office_file_formats/ms-pst/141923d5-15ab-4ef1-a524-6dce75aae546

typedef struct {
    uint64_t ibFileEof;
    uint64_t nbt_bid; 
    uint64_t nbt_ib;
    uint64_t bbt_bid;
    uint64_t bbt_ib;
} PstRoot;

typedef struct {
	uint32_t magic;
	uint16_t version; //0x000E ANSI o 0x0015 UNICODE
	uint8_t bCryptMethod;
	PstRoot root;
} PstHeader;

typedef struct{
	//rgentries 488 bytes
	uint8_t cEnt;		//number of BTree entries stored in the page data.
	uint8_t cEntMax;	//maximum number of entries that can fit inside the page data.
	uint8_t cbEnt; 		//size of each BTree entry, in bytes
	uint8_t cLevel;		//depth level of the page. Leaf pages have a level of zero, intermediate pages have a level greater than 0
	uint64_t startOffset;
	//padding 4 bytes
	//pageTrailer 16bytes
} PstBtPage;

typedef struct{
	uint64_t key;
	uint64_t bid;
	uint64_t ib;	//offset to data
	//8 bit padding
} PstBtPageEntry;

typedef struct {
    uint64_t nid; //nid & 0x1F to get the NID_TYPE value
    uint64_t bidData; 
    uint64_t bidSub;
    uint32_t nidParent; //If this node represents a child of a Folder object defined in the Messaging Layer, then this value is nonzero and contains the NID of the parent Folder object's node.
} PstNbtLeaf;

typedef struct {
    uint64_t bid; 
    uint64_t ib; 
    uint16_t cb;
    uint16_t cRef;
    //uint32_t dwPadding;
} PstBbtLeaf;

typedef struct {
	uint16_t ibHnpm;      //Offset Heap Page Map
    uint8_t  bSig;        //Must be 0xEC
    uint8_t  bClientSig;  //Type (es: PC, BTH, ecc.)
    uint32_t hidUserRoot; //root (entry point)
} PstHnHdr;

typedef struct {
    uint8_t  bType;
    uint8_t  cbKey;
    uint8_t  cbEnt;
    uint8_t  bIdxLevels; 
    uint32_t hidRoot;
} PstBthHeader;


typedef struct {
	uint16_t wPropId;
	uint16_t wPropType;
	uint32_t dwValueHnid;
} PstPcBthRecord;


typedef struct{
	uint16_t cAlloc;
	uint16_t cFree;
} PstHnPageMap;

typedef struct {
    uint64_t nid; 
    uint64_t bidData;
    uint64_t bidSub;
} PstSlEntry;

typedef struct {
	uint8_t btype; //Must be 0x02
	uint8_t cLevel; //Must be 0x00
	uint16_t cEnt; //Number of entries cannot be 0
	uint32_t dwPadding; //Must be 0
	//rgentries cEnt * 24 UNICODE (12 if ANSI)
	//rgbPadding (variable): This field is present if the total size of all of the other fields is not a multiple of 64. The size of this field is the smallest number of bytes required to make the size of the SLBLOCK a multiple of 64.
} PstSlBlock;

typedef struct{
	uint8_t btype; //Must be 0x02
	uint8_t cLevel; //Must be 0x01
	uint16_t cEnt;
	uint32_t dwPadding; //Must be zeroed
	//rgentries (UNICODE 16byte per entry or 8 ANSI)
	//rgbPadding 
	//blockTrailer 
} PstSiBlock;

typedef struct{
	uint8_t btype; //Must be 0x01
	uint8_t cLevel; //Must be 0x01
	uint16_t cEnt;
	uint32_t lcbTotal;
	//rgbids datablocks array
} PstXblock;

typedef struct{
	uint64_t nid;
	uint64_t bid;
} PstSiEntry;


//data handler
typedef void (*PstDataHandler)(PstNbtLeaf *leaf, PstPcBthRecord *rec, uint16_t size, uint8_t *buffer, void *ctx);

//public
void pst_stdout_stream_email_header(const char *filePath);
void pst_stdout_stream_emails_full_info(const char *filePath);

void pst_stdout_email_folder(const char *filePath, uint64_t nid);
void pst_stdout_stream_email_body(const char *filePath, uint64_t nid);
void pst_stdout_stream_email_full_info(const char *filePath, uint64_t nid);
void pst_extracxt_email_attachments(const char *filePath, uint64_t nid);

void pst_handle_nbt_leaf(PstNbtLeaf *leaf, PstHeader *hdr, FILE *fp, PstDataHandler cb, void *ctx);
void pst_attachment_dump_cb(
    PstNbtLeaf *leaf,
    PstPcBthRecord *rec,
    uint16_t cb,
    uint8_t *data,
    void *ctx
);

void pst_extract_attachment(
    uint64_t bidData,
    uint64_t bidSub,
    PstHeader *hdr,
    FILE *fp
);
#endif

