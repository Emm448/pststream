#include "stack.h"
#include "pst_io.h"
#include "pst_crypto.h"
#include "pst_types.h"
#include "pst.h"


#include <string.h>
#include <inttypes.h>

void pst_read_block(FILE *fp, uint64_t offset, uint8_t *buffer, uint64_t blockSize, uint8_t bCryptMethod){
	pst_seek(fp, offset, SEEK_SET);
	fread(buffer, sizeof(uint8_t), blockSize, fp);
	if(bCryptMethod == 0x01)
		pst_crypt_permute(buffer, blockSize, 0x00);
}

bool pst_parse_header(PstHeader *pstHeader, FILE* fp){
	pstHeader->magic = (uint32_t) read_byte(fp, 4, 0x00);
	pstHeader->version = (uint16_t) read_byte(fp, 2, 0x0A); //0X0E ANSI 0X17 unicode
	pstHeader->bCryptMethod = (uint8_t) read_byte(fp, 1, 0x201);
	return (pstHeader->magic == PST_MAGIC);
}

void pst_parse_root(PstRoot *pstRoot, FILE *fp){
	pstRoot->ibFileEof = (uint64_t) read_byte(fp, 8, 0xB8);
	pstRoot->nbt_bid = (uint64_t) read_byte(fp, 8, (0xC0 + 24)); //unused
	pstRoot->nbt_ib  = (uint64_t) read_byte(fp, 8, NO_OFFSET) & ~0xF; //Removing flag
	pstRoot->bbt_bid = (uint64_t) read_byte(fp, 8, NO_OFFSET);	  //unused
	pstRoot->bbt_ib  = (uint64_t) read_byte(fp, 8, NO_OFFSET) & ~0xF; //Removing flag
}

void pst_parse_bt_page(PstBtPage *pstBtPage, uint64_t pageOffset, FILE *fp){
	pstBtPage->startOffset = (uint64_t) pageOffset;
	pstBtPage->cEnt = (uint8_t) read_byte(fp, 1,  (pageOffset + 488) ); //assuming UNICODE
	pstBtPage->cEntMax = (uint8_t) read_byte(fp, 1, NO_OFFSET);
	pstBtPage->cbEnt = (uint8_t) read_byte(fp, 1, NO_OFFSET);
	pstBtPage->cLevel = (uint8_t) read_byte(fp, 1, NO_OFFSET);
}

void pst_parse_bt_entry(PstBtPageEntry *pstBtPageEntry, FILE *fp){
	pstBtPageEntry->key = (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstBtPageEntry->bid = (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstBtPageEntry->ib = (uint64_t) read_byte(fp, 8, NO_OFFSET);
}

void pst_parse_nbt_leaf(PstNbtLeaf *pstBtPageEntryLeaf,  FILE *fp){
	pstBtPageEntryLeaf->nid = (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstBtPageEntryLeaf->bidData = (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstBtPageEntryLeaf->bidSub = (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstBtPageEntryLeaf->nidParent = (uint32_t) read_byte(fp, 4, NO_OFFSET);
	read_byte(fp, 4, NO_OFFSET); //padding
}

void pst_parse_bbt_leaf(PstBbtLeaf *pstBbtLeaf, FILE *fp){
	pstBbtLeaf->bid = (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstBbtLeaf->ib = (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstBbtLeaf->cb = (uint16_t) read_byte(fp, 2, NO_OFFSET);
	pstBbtLeaf->cRef = (uint16_t) read_byte(fp, 2, NO_OFFSET);
	read_byte(fp, 4, NO_OFFSET); //padding
}

PstBbtLeaf pst_find_node_bbt(uint64_t rootPageOffset, uint64_t target, FILE *fp){
    uint64_t currPageOffset = rootPageOffset;
    while (1) {
        // Parsing Page
        PstBtPage currPage;
        pst_parse_bt_page(&currPage, currPageOffset, fp);

        if (currPage.cLevel > 0) {

            pst_seek(fp, currPage.startOffset, SEEK_SET);

            PstBtPageEntry curr;
            PstBtPageEntry prev;

            uint64_t nextChild = 0;
            int foundChild = 0;

            for (int i = 0; i < currPage.cEnt; i++) {
                pst_parse_bt_entry(&curr, fp);
                // PST B-tree logic:
                // first key >= target => take previous child
                if (curr.key >= target) {
                    if (i == 0) {
                        nextChild = curr.ib;
                    } else {
                        nextChild = prev.ib;
                    }
                    foundChild = 1;
                    break;
                }

                prev = curr;
            }

            if (!foundChild) {
                nextChild = curr.ib;
            }
			
            currPageOffset = nextChild;
        }

        else {
            pst_seek(fp, currPage.startOffset, SEEK_SET);
            PstBbtLeaf leaf;
            for (int i = 0; i < currPage.cEnt; i++) {
                pst_parse_bbt_leaf(&leaf, fp);
                if (leaf.bid == target) {
                    return leaf;
                }
            }
            break;
        }
    }
    return (PstBbtLeaf){0};
}

PstNbtLeaf pst_find_node_nbt(uint64_t rootPageOffset, uint64_t target, FILE *fp){
    uint64_t currPageOffset = rootPageOffset;

    while (1) {
        PstBtPage currPage;
        pst_parse_bt_page(&currPage, currPageOffset, fp);

        if (currPage.cLevel > 0) {

            pst_seek(fp, currPage.startOffset, SEEK_SET);

            PstBtPageEntry curr, prev;
            uint64_t nextChild = 0;
            int foundChild = 0;

            for (int i = 0; i < currPage.cEnt; i++) {
                pst_parse_bt_entry(&curr, fp);

                if (curr.key >= target) {
                    nextChild = (i == 0) ? curr.ib : prev.ib;
                    foundChild = 1;
                    break;
                }

                prev = curr;
            }

            if (!foundChild) {
                nextChild = curr.ib;
            }

            currPageOffset = nextChild;
        }

        else {
            pst_seek(fp, currPage.startOffset, SEEK_SET);

            PstNbtLeaf leaf;
            for (int i = 0; i < currPage.cEnt; i++) {
                pst_parse_nbt_leaf(&leaf, fp);

                if (leaf.nid == target) {
                    return leaf;
                }
            }

            break;
        }
    }

    return (PstNbtLeaf){0};
}

void pst_parse_hn_header(PstHnHdr *pstHnHdr, uint8_t *buffer, uint64_t startOffset){
	pstHnHdr->ibHnpm = (uint16_t) read_buffer(buffer, 2, startOffset);
	pstHnHdr->bSig = (uint8_t) read_buffer(buffer, 1, startOffset + 0x02);
	pstHnHdr->bClientSig = (uint8_t) read_buffer(buffer, 1, startOffset + 0x03);
	pstHnHdr->hidUserRoot = (uint32_t) read_buffer(buffer, 4, startOffset + 0x04);
}

void pst_parse_bth_header(PstBthHeader *pstBthHeader, uint8_t *buffer, uint64_t startOffset){
	pstBthHeader->bType = (uint8_t) read_buffer(buffer, 1, startOffset);
	pstBthHeader->cbKey = (uint8_t) read_buffer(buffer, 1, startOffset + 0x01);
	pstBthHeader->cbEnt = (uint8_t) read_buffer(buffer, 1, startOffset + 0x02);
	pstBthHeader->bIdxLevels = (uint8_t) read_buffer(buffer, 1, startOffset + 0x03);
	pstBthHeader->hidRoot = (uint32_t) read_buffer(buffer, 4, startOffset + 0x4);
}

void pst_parse_hn_pageMap(PstHnPageMap *pstHnPageMap, uint8_t *buffer, uint64_t startOffset){
	pstHnPageMap->cAlloc = read_buffer(buffer, 2, startOffset);
	pstHnPageMap->cFree = read_buffer(buffer, 2, startOffset + 0x02);
}

void pst_parse_sl_block(PstSlBlock *pstSlBlock, uint64_t offset, FILE* fp){
	pst_seek(fp, offset, SEEK_SET);
	pstSlBlock->btype = (uint8_t) read_byte(fp, 1, NO_OFFSET);
	pstSlBlock->cLevel = (uint8_t) read_byte(fp, 1, NO_OFFSET);
	pstSlBlock->cEnt = (uint16_t) read_byte(fp, 2, NO_OFFSET);
	pstSlBlock->dwPadding = (uint32_t) read_byte(fp, 4, NO_OFFSET);
}

void pst_parse_bth_record(PstPcBthRecord *pstPcBthRecord, uint64_t offset, uint8_t *buffer){
	pstPcBthRecord->wPropId = read_buffer(buffer, 2,  offset);
	pstPcBthRecord->wPropType = read_buffer(buffer, 2, offset + 2);
	pstPcBthRecord->dwValueHnid = read_buffer(buffer, 4, offset + 4);			
}

void pst_parse_sl_entry(PstSlEntry *pstSlEntry, FILE *fp){
	pstSlEntry->nid 	= (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstSlEntry->bidData	= (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstSlEntry->bidSub	= (uint64_t) read_byte(fp, 8, NO_OFFSET);
}

void pst_parse_xblock(PstXblock *pstXblock, FILE *fp, uint64_t startOffset){
	pstXblock->btype = (uint8_t) read_byte(fp, 1, startOffset);
	pstXblock->cLevel = (uint8_t) read_byte(fp, 1, startOffset + 0x01);
	pstXblock->cEnt = (uint16_t) read_byte(fp, 2, startOffset + 0x02);
	pstXblock->lcbTotal = (uint32_t) read_byte(fp, 4, startOffset + 0x04);
}

void pst_parse_si_entry(PstSiEntry *pstSiEntry, FILE *fp){
	pstSiEntry->nid = (uint64_t) read_byte(fp, 8, NO_OFFSET);
	pstSiEntry->bid = (uint64_t) read_byte(fp, 8, NO_OFFSET);
}

void pst_print_email_header_handler(PstNbtLeaf *leaf, PstPcBthRecord *rec, uint16_t size, uint8_t *buffer, void *ctx){
    if(size == 0) return;
	
	const char *label = NULL;

	if (rec->wPropId == PR_IMPORTANCE){
		printf("\n============================================================\n");
		label = "Importance : ";
	}
    else if (rec->wPropId == PR_MESSAGE_CLASS) {
		//ToDo, usare hash o cmpstr(const char* str, uint8_t *buff, bool isUTF16)
		//if(strcmp((char*)buffer, hash("IPM.Note")) == 0) printf("BODY\n");
		//if(strcmp((char*)buffer, hash("IPM.Appointment")) == 0) printf("APPOINTMENT\n");
		//if(strcmp((char*)buffer, hash("IPM.Contact") == 0)) printf("CONTACT\n");
        pst_print_property(rec->wPropType, buffer, size); //ToDo Remove
		printf("NID: %" PRIu64 "\n", leaf->nid); 
		printf("============================================================\n");
    }

    switch(rec->wPropId) {
		//case PR_IMPORTANCE:				label = "Importance : ";	break;
        case PR_SUBJECT:				label = "Subject    : ";	break;
		case PR_CLIENT_SUBMIT_TIME:		label = "Sent       : ";	break;
        case PR_SENDER:					label = "From       : ";	break;
        case PR_SENDER_NAME:			label = "FromName   : ";	break;
        case PR_DISPLAY_TO:				label = "To         : ";	break;
        case PR_DISPLAY_CC:				label = "Cc         : ";	break;
		case PR_MESSAGE_DELIVERY_TIME:	label = "Received   : ";	break;
		
		case PR_DISPLAY_NAME:{
			if((leaf->nid & 0x1F) == NID_TYPE_NORMAL_FOLDER)
				label = "Folder     : ";
			break;
		}
		
		case PR_MESSAGE_FLAGS:{
            printf("Flags      : ");
			uint32_t f = *(uint32_t*)buffer;
			if (f & MSGFLAG_READ)      printf("READ ");
			else                       printf("UNREAD ");
			if (f & MSGFLAG_UNSENT)    printf("DRAFT ");
			if (f & MSGFLAG_FROMME)    printf("FROMME ");
			if (f & MSGFLAG_HASATTACH) printf("ATTACH ");
            printf("\n");
            return;
		}	
    }

			
    if(!label) return;

    printf("%s", label);

	pst_print_property(rec->wPropType, buffer, size); 

}

void pst_print_email_body_handler(PstNbtLeaf *leaf, PstPcBthRecord *rec, uint16_t size, uint8_t *buffer, void *ctx){
    if(size == 0) return; 
	
	const char *label = NULL;

	switch(rec->wPropId){
		case PR_HTML:                   label = "\n-- HTML --\n";   break;
		case PR_RTF_COMPRESSED:         label = "\n-- RTF --\n";    break;
		case PR_BODY:                   label = "\n--- PLAIN ---\n"; break;
	}

	if(!label) return;
	
    printf("%s", label);

	pst_print_property(rec->wPropType, buffer, size);
}

void pst_print_email_full_handler(PstNbtLeaf *leaf, PstPcBthRecord *rec, uint16_t size, uint8_t *buffer, void *ctx){
	pst_print_email_header_handler(leaf, rec, size, buffer, ctx);
	pst_print_email_body_handler(leaf, rec, size, buffer, ctx);
}

uint32_t hid_index(uint32_t hid) {
    return (hid >> 5) - 1;
}

uint32_t hid_offset(uint32_t hid) {
    return hid & 0x1F;
}

bool pst_is_hid(uint32_t dwValueHnid, uint16_t nEntries){
	return ((dwValueHnid > 0) && (hid_index(dwValueHnid) < nEntries));
}

int pst_get_hid_bounds(const uint8_t *buffer, const PstHnHdr *hdr, uint32_t hid, uint16_t *start, uint16_t *end) {
    uint32_t index = hid_index(hid);

    const uint8_t *base = buffer + hdr->ibHnpm + 4;

    *start = read_le16(base + 2 * index);
    *end   = read_le16(base + 2 * (index + 1));

    if (*end < *start)
        return -1;

    return 0;
}

void pst_handle_inline_prop(PstNbtLeaf *leaf, PstPcBthRecord *rec, PstDataHandler cb, void *ctx) {
    uint32_t raw  = rec->dwValueHnid;
    uint16_t type = rec->wPropType;

    uint8_t buf[4];
    uint16_t size = 0;

    switch (type) {
        case PT_SHORT: { // 2 bytes
            int16_t v = (int16_t)(raw & 0xFFFF);
            memcpy(buf, &v, sizeof(v));
            size = sizeof(v);
            break;
        }

        case PT_LONG: { // 4 bytes
            int32_t v = (int32_t)raw;
            memcpy(buf, &v, sizeof(v));
            size = sizeof(v);
            break;
        }

        case PT_FLOAT: { // 4 bytes
            float v;
            memcpy(&v, &raw, sizeof(v));
            memcpy(buf, &v, sizeof(v));
            size = sizeof(v);
            break;
        }

        case PT_ERROR: { // 4 bytes
            memcpy(buf, &raw, sizeof(uint32_t));
            size = sizeof(uint32_t);
            break;
        }

        case PT_BOOLEAN: { // 2 bytes
            int16_t v = (raw & 0xFFFF) ? 1 : 0;
            memcpy(buf, &v, sizeof(v));
            size = sizeof(v);
            break;
        }

        default:
            return;
    }

    cb(leaf, rec, size, buf, ctx);
}

void pst_handle_hid_prop(PstNbtLeaf *leaf, uint8_t *buffer, const PstHnHdr *hdr, const PstHnPageMap *map, PstPcBthRecord *rec, PstDataHandler cb, void *ctx) {
    uint32_t hid = rec->dwValueHnid;
    uint32_t idx = hid_index(hid);

    if (idx >= map->cAlloc)
        return;

    uint16_t start, end;
    if (pst_get_hid_bounds(buffer, hdr, hid, &start, &end) != 0)
        return;

    uint16_t size = end - start;
    if (size == 0)
        return;
	
    cb(leaf, rec, size, buffer + start, ctx);
}

int pst_read_xblock_chain(FILE *fp, PstHeader *hdr, PstXblock *xb, uint8_t *out, size_t out_size) {

    size_t off = 0;

    for (int i = 0; i < xb->cEnt; i++) {
        uint64_t bid = read_byte(fp, 8, pst_tell(fp));
        long pos = pst_tell(fp);

        PstBbtLeaf leaf = pst_find_node_bbt(hdr->root.bbt_ib, bid, fp);
        if (leaf.ib == 0)
            continue;

        if (off + leaf.cb > out_size)
            return -1;

        pst_read_block(fp, leaf.ib, out + off, leaf.cb, hdr->bCryptMethod);
        off += leaf.cb;

        pst_seek(fp, pos, SEEK_SET);
    }
    return (int)off;
}

void pst_attachment_dump_cb(
    PstNbtLeaf *leaf,
    PstPcBthRecord *rec,
    uint16_t cb,
    uint8_t *data,
    void *ctx
){
    (void)leaf;
    (void)ctx;

    switch (rec->wPropId) {

        case 0x3704:
        case 0x3707:
            printf("\n[FILENAME PROP %04X]\n", rec->wPropId);
            //print_hex_dump(data, cb);
			print_utf16le(data, cb);
            break;

        case 0x3701: {
    printf("\n[ATTACHMENT DATA %u bytes]\n", cb);

    /* 1) save binary */
    char filename_bin[256];
    snprintf(filename_bin, sizeof(filename_bin),
             "attach_%08x.bin", rec->dwValueHnid);

    FILE *fb = fopen(filename_bin, "wb");
    if (fb) {
        fwrite(data, 1, cb, fb);
        fclose(fb);
        printf("Saved binary: %s\n", filename_bin);
    } else {
        perror("fopen bin");
    }

    /* 2) save HEX DUMP into file */
    char filename_hex[256];
    snprintf(filename_hex, sizeof(filename_hex),
             "attach_%08x.hex.txt", rec->dwValueHnid);

    FILE *fh = fopen(filename_hex, "w");
    if (fh) {
        for (size_t i = 0; i < cb; i += 16) {

            fprintf(fh, "%08zx  ", i);

            for (size_t j = 0; j < 16; j++) {
                if (i + j < cb)
                    fprintf(fh, "%02x ", data[i + j]);
                else
                    fprintf(fh, "   ");

                if (j == 7) fprintf(fh, " ");
            }

            fprintf(fh, " |");

            for (size_t j = 0; j < 16 && i + j < cb; j++) {
                unsigned char c = data[i + j];
                fprintf(fh, "%c", (c >= 32 && c <= 126) ? c : '.');
            }

            fprintf(fh, "|\n");
        }

        fclose(fh);
        printf("Saved hex dump: %s\n", filename_hex);
    }

    /* 3) debug */
    if (cb >= 4) {
        printf("MAGIC: %02X %02X %02X %02X\n",
               data[0], data[1], data[2], data[3]);
    }

    break;
}

        default:
            break;
    }
}

void pst_extract_attachment(
    uint64_t bidData,
    uint64_t bidSub,
    PstHeader *hdr,
    FILE *fp
){
    PstNbtLeaf leaf;

    leaf.bidData = bidData;
    leaf.bidSub  = bidSub;
    leaf.nid     = 0; // non useful for PC parsing

    pst_handle_nbt_leaf(
        &leaf,
        hdr,
        fp,
        pst_attachment_dump_cb,
        NULL
    );
}

void pst_handle_external_prop(PstNbtLeaf *leaf, PstPcBthRecord *rec, PstHeader *hdr, FILE *fp, PstDataHandler cb, void *ctx) {
    PstBbtLeaf sub = pst_find_node_bbt(hdr->root.bbt_ib, leaf->bidSub, fp);
    if (sub.ib == 0)
        return;

    PstSlBlock sl;
    pst_parse_sl_block(&sl, sub.ib, fp);

    for (int i = 0; i < sl.cEnt; i++) {
		
        if (sl.cLevel == 0x00) { // LBLOCK
            PstSlEntry e;
            pst_parse_sl_entry(&e, fp);
			
			if(ctx != NULL && *((int*)ctx) == 1){
				if((e.nid & 0x1F) == 0x05){
					pst_extract_attachment(e.bidData, e.bidSub, hdr, fp);
					continue;
				}
			}
			
            long pos = pst_tell(fp);

            if (rec->dwValueHnid != e.nid) {
                pst_seek(fp, pos, SEEK_SET);
                continue;
            }

            PstBbtLeaf data = pst_find_node_bbt(hdr->root.bbt_ib, e.bidData, fp);
            if (data.ib == 0) {
                pst_seek(fp, pos, SEEK_SET);
                continue;
            }

            int blockId    = read_byte(fp, 1, data.ib);
            int blockLevel = read_byte(fp, 1, data.ib + 1);

            if (blockId == 0x01 && blockLevel == 0x01) { //XBLOCK
			    PstXblock xb;
				pst_parse_xblock(&xb, fp, data.ib);
			
				uint8_t buffer[xb.lcbTotal];
				int written = pst_read_xblock_chain(fp, hdr, &xb, buffer, sizeof(buffer));
				if (written > 0)
					cb(leaf, rec, written, buffer, ctx);
			
            }else{
                uint8_t buffer[data.cb];
                pst_read_block(fp, data.ib, buffer, data.cb, hdr->bCryptMethod);
                cb(leaf, rec, data.cb, buffer, ctx);
            }

            pst_seek(fp, pos, SEEK_SET);
        }

        else if (sl.cLevel == 0x01) { //IBLOCK
            PstSiEntry si;
            pst_parse_si_entry(&si, fp);
		
            long pos = pst_tell(fp);

            if (rec->dwValueHnid != si.nid) {
                pst_seek(fp, pos, SEEK_SET);
                continue;
            }

            PstBbtLeaf data = pst_find_node_bbt(hdr->root.bbt_ib, si.bid, fp);
            if (data.ib == 0) {
                pst_seek(fp, pos, SEEK_SET);
                continue;
            }

            uint8_t buffer[data.cb];
            pst_read_block(fp, data.ib, buffer, data.cb, hdr->bCryptMethod);

            cb(leaf, rec, data.cb, buffer, ctx);

            pst_seek(fp, pos, SEEK_SET);
        }
    }
}

bool pst_is_inline_type(uint16_t type) {
    switch (type) {
        case PT_NULL: 
        case PT_SHORT: 
        case PT_LONG: 
        case PT_FLOAT:
        case PT_BOOLEAN:
            return true;
        default:
            return false;
    }
}

void pst_handle_nbt_leaf(PstNbtLeaf *leaf, PstHeader *hdr, FILE *fp, PstDataHandler cb, void *ctx) {
	PstBbtLeaf data = pst_find_node_bbt(hdr->root.bbt_ib, leaf->bidData, fp);
    if (data.ib == 0)
        return;
	
	
    uint8_t buffer[data.cb];
    pst_read_block(fp, data.ib, buffer, data.cb, hdr->bCryptMethod);
	
    PstHnHdr hn;
    pst_parse_hn_header(&hn, buffer, 0);

    if (hn.bSig != 0xEC || hn.bClientSig != 0xBC) //Only Property Context!
        return;


    PstHnPageMap map;
    pst_parse_hn_pageMap(&map, buffer, hn.ibHnpm);

    uint16_t rootOff = read_le16(
        buffer + hn.ibHnpm + 4 + 2 * hid_index(hn.hidUserRoot)
    );

    PstBthHeader bth;
    pst_parse_bth_header(&bth, buffer, rootOff);

    if (bth.bType != 0xB5 || bth.bIdxLevels != 0)
        return;
	
    uint32_t idx = hid_index(bth.hidRoot);

    uint16_t start = read_le16(buffer + hn.ibHnpm + 4 + 2 * idx);
    uint16_t end   = read_le16(buffer + hn.ibHnpm + 4 + 2 * (idx + 1));

    uint16_t entrySize = bth.cbKey + bth.cbEnt;
    uint16_t nEntries  = (end - start) / entrySize;
	
    for (int i = 0; i < nEntries; i++) {

        PstPcBthRecord rec;
        pst_parse_bth_record(&rec, start + i * entrySize, buffer);		
		
        if (pst_is_inline_type(rec.wPropType)) {
            pst_handle_inline_prop(leaf, &rec, cb, ctx);
        } else if(pst_is_hid(rec.dwValueHnid, map.cAlloc)){
			pst_handle_hid_prop(leaf, buffer, &hn, &map, &rec, cb, ctx);
		}
		else{
            pst_handle_external_prop(leaf, &rec, hdr, fp, cb, ctx);
        }
    }
}

void pst_traverse_nbt(PstHeader *pstHeader, FILE *fp, PstDataHandler callback, void *ctx, uint32_t nidFilter){
	uint64_t rootPageOffset = pstHeader->root.nbt_ib;
	Stack stack; 
	stack_init(&stack);
	stack_push(&stack, rootPageOffset);
	
	while(stack.size > 0){
		uint64_t currPageOffset = (uint64_t) stack_pop(&stack); 
		//Parsing Page
		PstBtPage currPage;
		pst_parse_bt_page(&currPage, currPageOffset, fp);
		
		if(currPage.cLevel > 0){
			//Parsing entries
			pst_seek(fp, currPage.startOffset, SEEK_SET);
			PstBtPageEntry pstBtPageEntry;
			for(int i = 0; i < currPage.cEnt; i++){
				pst_parse_bt_entry(&pstBtPageEntry, fp);
				stack_push(&stack, pstBtPageEntry.ib);
			}
		}else{
			//Parsing leafs
			long pos = currPage.startOffset;
			PstNbtLeaf pstNbtLeaf;
			for(int i = 0; i < currPage.cEnt; i++){
				pst_seek(fp, pos, SEEK_SET);
				pst_parse_nbt_leaf(&pstNbtLeaf, fp);
				pos = pst_tell(fp);
				if((nidFilter != NID_TYPE_NULL) && (nidFilter != (pstNbtLeaf.nid & 0x1F))) continue;
				if(callback) pst_handle_nbt_leaf(&pstNbtLeaf, pstHeader, fp, callback, ctx);
			}
		}
	}
}

void pst_stdout_stream_email_header(const char *filePath){
	FILE *fp = fopen(filePath, "rb");
		
	PstHeader pstHeader;
	pst_parse_header(&pstHeader, fp);
	pst_parse_root(&pstHeader.root, fp);
	
	pst_traverse_nbt(&pstHeader, fp, &pst_print_email_header_handler, NULL, NID_TYPE_NORMAL_MESSAGE);
	fclose(fp);
}

void pst_stdout_stream_emails_full_info(const char *filePath){
	FILE *fp = fopen(filePath, "rb");
		
	PstHeader pstHeader;
	pst_parse_header(&pstHeader, fp);
	pst_parse_root(&pstHeader.root, fp);
	
	pst_traverse_nbt(&pstHeader, fp, &pst_print_email_full_handler, NULL, NID_TYPE_NORMAL_MESSAGE);
	fclose(fp);
}

void pst_stdout_stream_email_body(const char *filePath, uint64_t nid){
	FILE *fp = fopen(filePath, "rb");
		
	PstHeader pstHeader;
	pst_parse_header(&pstHeader, fp);
	pst_parse_root(&pstHeader.root, fp);
	
	//Find email nid
	PstNbtLeaf pstNbtLeaf = pst_find_node_nbt(pstHeader.root.nbt_ib, nid, fp);

	//Get Mail Body
	pst_handle_nbt_leaf(&pstNbtLeaf, &pstHeader, fp, &pst_print_email_body_handler, NULL);
	
	fclose(fp);
}

void pst_stdout_stream_email_full_info(const char *filePath, uint64_t nid){
	FILE *fp = fopen(filePath, "rb");
		
	PstHeader pstHeader;
	pst_parse_header(&pstHeader, fp);
	pst_parse_root(&pstHeader.root, fp);
	
	//Find email nid
	PstNbtLeaf pstNbtLeaf = pst_find_node_nbt(pstHeader.root.nbt_ib, nid, fp);
	
	//Get Mail header
	pst_handle_nbt_leaf(&pstNbtLeaf, &pstHeader, fp, &pst_print_email_header_handler, NULL);
	
	//Get Mail Folder
	PstNbtLeaf pstParentLeaf = pst_find_node_nbt(pstHeader.root.nbt_ib, pstNbtLeaf.nidParent, fp);
	pst_handle_nbt_leaf(&pstParentLeaf, &pstHeader, fp, &pst_print_email_header_handler, NULL);
	
	//Get Mail Body
	pst_handle_nbt_leaf(&pstNbtLeaf, &pstHeader, fp, &pst_print_email_body_handler, NULL);
	
	fclose(fp);
}

void pst_stdout_email_folder(const char *filePath, uint64_t nid){
	FILE *fp = fopen(filePath, "rb");
		
	PstHeader pstHeader;
	pst_parse_header(&pstHeader, fp);
	pst_parse_root(&pstHeader.root, fp);
	
	PstNbtLeaf pstNbtLeaf = pst_find_node_nbt(pstHeader.root.nbt_ib, nid, fp);
	PstNbtLeaf pstParentLeaf = pst_find_node_nbt(pstHeader.root.nbt_ib, pstNbtLeaf.nidParent, fp);
	pst_handle_nbt_leaf(&pstParentLeaf, &pstHeader, fp, &pst_print_email_header_handler, NULL);
	fclose(fp);
}

void pst_extracxt_email_attachments(const char *filePath, uint64_t nid){
	FILE *fp = fopen(filePath, "rb");
		
	PstHeader pstHeader;
	pst_parse_header(&pstHeader, fp);
	pst_parse_root(&pstHeader.root, fp);
	
	//Find email nid
	PstNbtLeaf pstNbtLeaf = pst_find_node_nbt(pstHeader.root.nbt_ib, nid, fp);

	//Get Mail Body
	int hasAtt = 1;
	pst_handle_nbt_leaf(&pstNbtLeaf, &pstHeader, fp, &pst_print_email_body_handler, &hasAtt);
	
	fclose(fp);
}
