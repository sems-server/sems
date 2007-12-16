#ifndef _DIAM_AVP_H
#define _DIAM_AVP_H

/* AVPS
 */


#define AAACreateAndAddAVPToMessage(_msg_,_code_,_flags_,_vdr_,_data_,_len_) \
	( AAAAddAVPToMessage(_msg_, \
	AAACreateAVP(_code_,_flags_,_vdr_,_data_,_len_, AVP_DUPLICATE_DATA),\
	(_msg_)->avpList.tail) )

AAA_AVP* AAACreateAVP(
		AAA_AVPCode code,
		AAA_AVPFlag flags,
		AAAVendorId vendorId,
		const char *data,
		unsigned int length,
		AVPDataStatus data_status);

AAA_AVP* AAACloneAVP(
		AAA_AVP *avp,
		unsigned char duplicate_data );

AAAReturnCode AAAAddAVPToMessage(
		AAAMessage *msg,
		AAA_AVP *avp,
		AAA_AVP *position);

AAA_AVP *AAAFindMatchingAVP(
		AAAMessage *msg,
		AAA_AVP *startAvp,
		AAA_AVPCode avpCode,
		AAAVendorId vendorId,
		AAASearchType searchType);

AAAReturnCode AAARemoveAVPFromMessage(
		AAAMessage *msg,
		AAA_AVP *avp);

AAAReturnCode AAAFreeAVP(
		AAA_AVP **avp);

AAA_AVP* AAAGetFirstAVP(
		AAA_AVP_LIST *avpList);

AAA_AVP* AAAGetLastAVP(
		AAA_AVP_LIST *avpList);

AAA_AVP* AAAGetNextAVP(
		AAA_AVP *avp);

AAA_AVP* AAAGetPrevAVP(
		AAA_AVP *avp);

char *AAAConvertAVPToString(
		AAA_AVP *avp,
		char *dest,
		unsigned int destLen);

AAA_AVP* AAAAddGroupedAVP(AAA_AVP* grouped, AAA_AVP* avp);

#endif
