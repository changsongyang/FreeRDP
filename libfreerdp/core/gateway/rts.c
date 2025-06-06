/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Request To Send (RTS) PDUs
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/crypto.h>

#include <freerdp/log.h>

#include "ncacn_http.h"
#include "rpc_client.h"
#include "rts_signature.h"

#include "rts.h"

#define TAG FREERDP_TAG("core.gateway.rts")

/**
 * RTS PDU Header
 *
 * The RTS PDU Header has the same layout as the common header of the connection-oriented RPC
 * PDU as specified in [C706] section 12.6.1, with a few additional requirements around the contents
 * of the header fields. The additional requirements are as follows:
 *
 * All fields MUST use little-endian byte order.
 *
 * Fragmentation MUST NOT occur for an RTS PDU.
 *
 * PFC_FIRST_FRAG and PFC_LAST_FRAG MUST be present in all RTS PDUs, and all other PFC flags
 * MUST NOT be present.
 *
 * The rpc_vers and rpc_vers_minor fields MUST contain version information as described in
 * [MS-RPCE] section 1.7.
 *
 * PTYPE MUST be set to a value of 20 (0x14). This field differentiates RTS packets from other RPC
 * packets.
 *
 * The packed_drep MUST indicate little-endian integer and floating-pointer byte order, IEEE
 * float-point format representation, and ASCII character format as specified in [C706]
 * section 12.6.
 *
 * The auth_length MUST be set to 0.
 *
 * The frag_length field MUST reflect the size of the header plus the size of all commands,
 * including the variable portion of variable-sized commands.
 *
 * The call_id MUST be set to 0 by senders and MUST be 0 on receipt.
 *
 */

static int rts_destination_command_read(rdpRpc* rpc, wStream* buffer, UINT32* Destination);

static const char* rts_command_to_string(UINT32 cmd, char* buffer, size_t len)
{
	const char* str = NULL;

#undef ENTRY
#define ENTRY(x)    \
	case x:         \
		str = "#x"; \
		break

	switch (cmd)
	{
		ENTRY(RTS_CMD_RECEIVE_WINDOW_SIZE);
		ENTRY(RTS_CMD_FLOW_CONTROL_ACK);
		ENTRY(RTS_CMD_CONNECTION_TIMEOUT);
		ENTRY(RTS_CMD_COOKIE);
		ENTRY(RTS_CMD_CHANNEL_LIFETIME);
		ENTRY(RTS_CMD_CLIENT_KEEPALIVE);
		ENTRY(RTS_CMD_VERSION);
		ENTRY(RTS_CMD_EMPTY);
		ENTRY(RTS_CMD_PADDING);
		ENTRY(RTS_CMD_NEGATIVE_ANCE);
		ENTRY(RTS_CMD_ANCE);
		ENTRY(RTS_CMD_CLIENT_ADDRESS);
		ENTRY(RTS_CMD_ASSOCIATION_GROUP_ID);
		ENTRY(RTS_CMD_DESTINATION);
		ENTRY(RTS_CMD_PING_TRAFFIC_SENT_NOTIFY);
		ENTRY(RTS_CMD_LAST_ID);
		default:
			str = "RTS_CMD_UNKNOWN";
			break;
	}

#undef ENTRY

	(void)_snprintf(buffer, len, "%s [0x%08" PRIx32 "]", str, cmd);
	return buffer;
}

static const char* rts_pdu_ptype_to_string(UINT32 ptype)
{
	switch (ptype)
	{
		case PTYPE_REQUEST:
			return "PTYPE_REQUEST";
		case PTYPE_PING:
			return "PTYPE_PING";
		case PTYPE_RESPONSE:
			return "PTYPE_RESPONSE";
		case PTYPE_FAULT:
			return "PTYPE_FAULT";
		case PTYPE_WORKING:
			return "PTYPE_WORKING";
		case PTYPE_NOCALL:
			return "PTYPE_NOCALL";
		case PTYPE_REJECT:
			return "PTYPE_REJECT";
		case PTYPE_ACK:
			return "PTYPE_ACK";
		case PTYPE_CL_CANCEL:
			return "PTYPE_CL_CANCEL";
		case PTYPE_FACK:
			return "PTYPE_FACK";
		case PTYPE_CANCEL_ACK:
			return "PTYPE_CANCEL_ACK";
		case PTYPE_BIND:
			return "PTYPE_BIND";
		case PTYPE_BIND_ACK:
			return "PTYPE_BIND_ACK";
		case PTYPE_BIND_NAK:
			return "PTYPE_BIND_NAK";
		case PTYPE_ALTER_CONTEXT:
			return "PTYPE_ALTER_CONTEXT";
		case PTYPE_ALTER_CONTEXT_RESP:
			return "PTYPE_ALTER_CONTEXT_RESP";
		case PTYPE_RPC_AUTH_3:
			return "PTYPE_RPC_AUTH_3";
		case PTYPE_SHUTDOWN:
			return "PTYPE_SHUTDOWN";
		case PTYPE_CO_CANCEL:
			return "PTYPE_CO_CANCEL";
		case PTYPE_ORPHANED:
			return "PTYPE_ORPHANED";
		case PTYPE_RTS:
			return "PTYPE_RTS";
		default:
			return "UNKNOWN";
	}
}

static rpcconn_rts_hdr_t rts_pdu_header_init(void)
{
	rpcconn_rts_hdr_t header = { 0 };
	header.header.rpc_vers = 5;
	header.header.rpc_vers_minor = 0;
	header.header.ptype = PTYPE_RTS;
	header.header.packed_drep[0] = 0x10;
	header.header.packed_drep[1] = 0x00;
	header.header.packed_drep[2] = 0x00;
	header.header.packed_drep[3] = 0x00;
	header.header.pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
	header.header.auth_length = 0;
	header.header.call_id = 0;

	return header;
}

static BOOL rts_align_stream(wStream* s, size_t alignment, BOOL silent)
{
	size_t pos = 0;
	size_t pad = 0;

	WINPR_ASSERT(s);
	WINPR_ASSERT(alignment > 0);

	pos = Stream_GetPosition(s);
	pad = rpc_offset_align(&pos, alignment);
	return Stream_ConditionalSafeSeek(s, pad, silent);
}

static char* sdup(const void* src, size_t length)
{
	char* dst = NULL;
	WINPR_ASSERT(src || (length == 0));
	if (length == 0)
		return NULL;

	dst = calloc(length + 1, sizeof(char));
	if (!dst)
		return NULL;
	memcpy(dst, src, length);
	return dst;
}

static BOOL rts_write_common_pdu_header(wStream* s, const rpcconn_common_hdr_t* header)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);
	if (!Stream_EnsureRemainingCapacity(s, sizeof(rpcconn_common_hdr_t)))
		return FALSE;

	Stream_Write_UINT8(s, header->rpc_vers);
	Stream_Write_UINT8(s, header->rpc_vers_minor);
	Stream_Write_UINT8(s, header->ptype);
	Stream_Write_UINT8(s, header->pfc_flags);
	Stream_Write(s, header->packed_drep, ARRAYSIZE(header->packed_drep));
	Stream_Write_UINT16(s, header->frag_length);
	Stream_Write_UINT16(s, header->auth_length);
	Stream_Write_UINT32(s, header->call_id);
	return TRUE;
}

BOOL rts_read_common_pdu_header(wStream* s, rpcconn_common_hdr_t* header, BOOL ignoreErrors)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	if (!ignoreErrors)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(rpcconn_common_hdr_t)))
			return FALSE;
	}
	else
	{
		const size_t sz = Stream_GetRemainingLength(s);
		if (sz < sizeof(rpcconn_common_hdr_t))
			return FALSE;
	}

	Stream_Read_UINT8(s, header->rpc_vers);
	Stream_Read_UINT8(s, header->rpc_vers_minor);
	Stream_Read_UINT8(s, header->ptype);
	Stream_Read_UINT8(s, header->pfc_flags);
	Stream_Read(s, header->packed_drep, ARRAYSIZE(header->packed_drep));
	Stream_Read_UINT16(s, header->frag_length);
	Stream_Read_UINT16(s, header->auth_length);
	Stream_Read_UINT32(s, header->call_id);

	if (header->frag_length < sizeof(rpcconn_common_hdr_t))
	{
		if (!ignoreErrors)
			WLog_WARN(TAG, "Invalid header->frag_length of %" PRIu16 ", expected %" PRIuz,
			          header->frag_length, sizeof(rpcconn_common_hdr_t));
		return FALSE;
	}

	if (!ignoreErrors)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s,
		                                      header->frag_length - sizeof(rpcconn_common_hdr_t)))
			return FALSE;
	}
	else
	{
		const size_t sz2 = Stream_GetRemainingLength(s);
		if (sz2 < header->frag_length - sizeof(rpcconn_common_hdr_t))
			return FALSE;
	}
	return TRUE;
}

static BOOL rts_read_auth_verifier_no_checks(wStream* s, auth_verifier_co_t* auth,
                                             const rpcconn_common_hdr_t* header, size_t* startPos,
                                             BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(auth);
	WINPR_ASSERT(header);

	WINPR_ASSERT(header->frag_length > header->auth_length + 8);

	if (startPos)
		*startPos = Stream_GetPosition(s);

	/* Read the auth verifier and check padding matches frag_length */
	{
		const size_t expected = header->frag_length - header->auth_length - 8;

		Stream_SetPosition(s, expected);
		if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 8, silent))
			return FALSE;

		Stream_Read_UINT8(s, auth->auth_type);
		Stream_Read_UINT8(s, auth->auth_level);
		Stream_Read_UINT8(s, auth->auth_pad_length);
		Stream_Read_UINT8(s, auth->auth_reserved);
		Stream_Read_UINT32(s, auth->auth_context_id);
	}

	if (header->auth_length != 0)
	{
		const void* ptr = Stream_Pointer(s);
		if (!Stream_ConditionalSafeSeek(s, header->auth_length, silent))
			return FALSE;
		auth->auth_value = (BYTE*)sdup(ptr, header->auth_length);
		if (auth->auth_value == NULL)
			return FALSE;
	}

	return TRUE;
}

static BOOL rts_read_auth_verifier(wStream* s, auth_verifier_co_t* auth,
                                   const rpcconn_common_hdr_t* header, BOOL silent)
{
	size_t pos = 0;
	WINPR_ASSERT(s);
	WINPR_ASSERT(auth);
	WINPR_ASSERT(header);

	if (!rts_read_auth_verifier_no_checks(s, auth, header, &pos, silent))
		return FALSE;

	const size_t expected = header->frag_length - header->auth_length - 8;
	WINPR_ASSERT(pos + auth->auth_pad_length == expected);
	return pos + auth->auth_pad_length == expected;
}

static BOOL rts_read_auth_verifier_with_stub(wStream* s, auth_verifier_co_t* auth,
                                             rpcconn_common_hdr_t* header, BOOL silent)
{
	size_t pos = 0;
	size_t alloc_hint = 0;
	BYTE** ptr = NULL;

	if (!rts_read_auth_verifier_no_checks(s, auth, header, &pos, silent))
		return FALSE;

	switch (header->ptype)
	{
		case PTYPE_FAULT:
		{
			rpcconn_fault_hdr_t* hdr = (rpcconn_fault_hdr_t*)header;
			alloc_hint = hdr->alloc_hint;
			ptr = &hdr->stub_data;
		}
		break;
		case PTYPE_RESPONSE:
		{
			rpcconn_response_hdr_t* hdr = (rpcconn_response_hdr_t*)header;
			alloc_hint = hdr->alloc_hint;
			ptr = &hdr->stub_data;
		}
		break;
		case PTYPE_REQUEST:
		{
			rpcconn_request_hdr_t* hdr = (rpcconn_request_hdr_t*)header;
			alloc_hint = hdr->alloc_hint;
			ptr = &hdr->stub_data;
		}
		break;
		default:
			return FALSE;
	}

	if (alloc_hint > 0)
	{
		const size_t off = header->auth_length + 8 + auth->auth_pad_length + pos;
		const size_t size = header->frag_length - MIN(header->frag_length, off);
		const void* src = Stream_Buffer(s) + pos;

		if (off > header->frag_length)
			WLog_WARN(TAG,
			          "Unexpected alloc_hint(%" PRIuz ") for PDU %s: size %" PRIuz
			          ", frag_length %" PRIu16 ", offset %" PRIuz,
			          alloc_hint, rts_pdu_ptype_to_string(header->ptype), size, header->frag_length,
			          off);

		*ptr = NULL;
		if (size > 0)
		{
			*ptr = (BYTE*)sdup(src, size);
			if (!*ptr)
				return FALSE;
		}
	}

	return TRUE;
}

static void rts_free_auth_verifier(auth_verifier_co_t* auth)
{
	if (!auth)
		return;
	free(auth->auth_value);
}

static BOOL rts_write_auth_verifier(wStream* s, const auth_verifier_co_t* auth,
                                    const rpcconn_common_hdr_t* header)
{
	size_t pos = 0;
	UINT8 auth_pad_length = 0;

	WINPR_ASSERT(s);
	WINPR_ASSERT(auth);
	WINPR_ASSERT(header);

	/* Align start to a multiple of 4 */
	pos = Stream_GetPosition(s);
	if ((pos % 4) != 0)
	{
		auth_pad_length = 4 - (pos % 4);
		if (!Stream_EnsureRemainingCapacity(s, auth_pad_length))
			return FALSE;
		Stream_Zero(s, auth_pad_length);
	}

#if defined(WITH_VERBOSE_WINPR_ASSERT) && (WITH_VERBOSE_WINPR_ASSERT != 0)
	WINPR_ASSERT(header->frag_length + 8ull > header->auth_length);
	{
		size_t apos = Stream_GetPosition(s);
		size_t expected = header->frag_length - header->auth_length - 8;

		WINPR_ASSERT(apos == expected);
	}
#endif

	if (!Stream_EnsureRemainingCapacity(s, sizeof(auth_verifier_co_t)))
		return FALSE;

	Stream_Write_UINT8(s, auth->auth_type);
	Stream_Write_UINT8(s, auth->auth_level);
	Stream_Write_UINT8(s, auth_pad_length);
	Stream_Write_UINT8(s, 0); /* auth->auth_reserved */
	Stream_Write_UINT32(s, auth->auth_context_id);

	if (!Stream_EnsureRemainingCapacity(s, header->auth_length))
		return FALSE;
	Stream_Write(s, auth->auth_value, header->auth_length);
	return TRUE;
}

static BOOL rts_read_version(wStream* s, p_rt_version_t* version, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(version);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 2 * sizeof(UINT8), silent))
		return FALSE;
	Stream_Read_UINT8(s, version->major);
	Stream_Read_UINT8(s, version->minor);
	return TRUE;
}

static void rts_free_supported_versions(p_rt_versions_supported_t* versions)
{
	if (!versions)
		return;
	free(versions->p_protocols);
	versions->p_protocols = NULL;
}

static BOOL rts_read_supported_versions(wStream* s, p_rt_versions_supported_t* versions,
                                        BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(versions);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, sizeof(UINT8), silent))
		return FALSE;

	Stream_Read_UINT8(s, versions->n_protocols); /* count */

	if (versions->n_protocols > 0)
	{
		versions->p_protocols = calloc(versions->n_protocols, sizeof(p_rt_version_t));
		if (!versions->p_protocols)
			return FALSE;
	}
	for (BYTE x = 0; x < versions->n_protocols; x++)
	{
		p_rt_version_t* version = &versions->p_protocols[x];
		if (!rts_read_version(s, version, silent)) /* size_is(n_protocols) */
		{
			rts_free_supported_versions(versions);
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL rts_read_port_any(wStream* s, port_any_t* port, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(port);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, sizeof(UINT16), silent))
		return FALSE;

	Stream_Read_UINT16(s, port->length);
	if (port->length == 0)
		return TRUE;

	const void* ptr = Stream_ConstPointer(s);
	if (!Stream_ConditionalSafeSeek(s, port->length, silent))
		return FALSE;
	port->port_spec = sdup(ptr, port->length);
	return port->port_spec != NULL;
}

static void rts_free_port_any(port_any_t* port)
{
	if (!port)
		return;
	free(port->port_spec);
}

static BOOL rts_read_uuid(wStream* s, p_uuid_t* uuid, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(uuid);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, sizeof(p_uuid_t), silent))
		return FALSE;

	Stream_Read_UINT32(s, uuid->time_low);
	Stream_Read_UINT16(s, uuid->time_mid);
	Stream_Read_UINT16(s, uuid->time_hi_and_version);
	Stream_Read_UINT8(s, uuid->clock_seq_hi_and_reserved);
	Stream_Read_UINT8(s, uuid->clock_seq_low);
	Stream_Read(s, uuid->node, ARRAYSIZE(uuid->node));
	return TRUE;
}

static BOOL rts_write_uuid(wStream* s, const p_uuid_t* uuid)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(uuid);

	if (!Stream_EnsureRemainingCapacity(s, sizeof(p_uuid_t)))
		return FALSE;

	Stream_Write_UINT32(s, uuid->time_low);
	Stream_Write_UINT16(s, uuid->time_mid);
	Stream_Write_UINT16(s, uuid->time_hi_and_version);
	Stream_Write_UINT8(s, uuid->clock_seq_hi_and_reserved);
	Stream_Write_UINT8(s, uuid->clock_seq_low);
	Stream_Write(s, uuid->node, ARRAYSIZE(uuid->node));
	return TRUE;
}

static p_syntax_id_t* rts_syntax_id_new(size_t count)
{
	return calloc(count, sizeof(p_syntax_id_t));
}

static void rts_syntax_id_free(p_syntax_id_t* ptr)
{
	free(ptr);
}

static BOOL rts_read_syntax_id(wStream* s, p_syntax_id_t* syntax_id, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(syntax_id);

	if (!rts_read_uuid(s, &syntax_id->if_uuid, silent))
		return FALSE;

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 4, silent))
		return FALSE;

	Stream_Read_UINT32(s, syntax_id->if_version);
	return TRUE;
}

static BOOL rts_write_syntax_id(wStream* s, const p_syntax_id_t* syntax_id)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(syntax_id);

	if (!rts_write_uuid(s, &syntax_id->if_uuid))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;

	Stream_Write_UINT32(s, syntax_id->if_version);
	return TRUE;
}

static void rts_context_elem_free(p_cont_elem_t* ptr)
{
	if (!ptr)
		return;
	rts_syntax_id_free(ptr->transfer_syntaxes);
	free(ptr);
}

WINPR_ATTR_MALLOC(rts_context_elem_free, 1)
static p_cont_elem_t* rts_context_elem_new(size_t count)
{
	p_cont_elem_t* ctx = calloc(count, sizeof(p_cont_elem_t));
	return ctx;
}

static BOOL rts_read_context_elem(wStream* s, p_cont_elem_t* element, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(element);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 4, silent))
		return FALSE;

	Stream_Read_UINT16(s, element->p_cont_id);
	Stream_Read_UINT8(s, element->n_transfer_syn); /* number of items */
	Stream_Read_UINT8(s, element->reserved);       /* alignment pad, m.b.z. */

	if (!rts_read_syntax_id(s, &element->abstract_syntax, silent)) /* transfer syntax list */
		return FALSE;

	if (element->n_transfer_syn > 0)
	{
		element->transfer_syntaxes = rts_syntax_id_new(element->n_transfer_syn);
		if (!element->transfer_syntaxes)
			return FALSE;
		for (BYTE x = 0; x < element->n_transfer_syn; x++)
		{
			p_syntax_id_t* syn = &element->transfer_syntaxes[x];
			if (!rts_read_syntax_id(s, syn, silent)) /* size_is(n_transfer_syn) */
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL rts_write_context_elem(wStream* s, const p_cont_elem_t* element)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(element);

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;
	Stream_Write_UINT16(s, element->p_cont_id);
	Stream_Write_UINT8(s, element->n_transfer_syn);         /* number of items */
	Stream_Write_UINT8(s, element->reserved);               /* alignment pad, m.b.z. */
	if (!rts_write_syntax_id(s, &element->abstract_syntax)) /* transfer syntax list */
		return FALSE;

	for (BYTE x = 0; x < element->n_transfer_syn; x++)
	{
		const p_syntax_id_t* syn = &element->transfer_syntaxes[x];
		if (!rts_write_syntax_id(s, syn)) /* size_is(n_transfer_syn) */
			return FALSE;
	}

	return TRUE;
}

static BOOL rts_read_context_list(wStream* s, p_cont_list_t* list, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(list);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 4, silent))
		return FALSE;
	Stream_Read_UINT8(s, list->n_context_elem); /* number of items */
	Stream_Read_UINT8(s, list->reserved);       /* alignment pad, m.b.z. */
	Stream_Read_UINT16(s, list->reserved2);     /* alignment pad, m.b.z. */

	if (list->n_context_elem > 0)
	{
		list->p_cont_elem = rts_context_elem_new(list->n_context_elem);
		if (!list->p_cont_elem)
			return FALSE;
		for (BYTE x = 0; x < list->n_context_elem; x++)
		{
			p_cont_elem_t* element = &list->p_cont_elem[x];
			if (!rts_read_context_elem(s, element, silent))
				return FALSE;
		}
	}
	return TRUE;
}

static void rts_free_context_list(p_cont_list_t* list)
{
	if (!list)
		return;
	rts_context_elem_free(list->p_cont_elem);
}

static BOOL rts_write_context_list(wStream* s, const p_cont_list_t* list)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(list);

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;
	Stream_Write_UINT8(s, list->n_context_elem); /* number of items */
	Stream_Write_UINT8(s, 0);                    /* alignment pad, m.b.z. */
	Stream_Write_UINT16(s, 0);                   /* alignment pad, m.b.z. */

	for (BYTE x = 0; x < list->n_context_elem; x++)
	{
		const p_cont_elem_t* element = &list->p_cont_elem[x];
		if (!rts_write_context_elem(s, element))
			return FALSE;
	}
	return TRUE;
}

static p_result_t* rts_result_new(size_t count)
{
	return calloc(count, sizeof(p_result_t));
}

static void rts_result_free(p_result_t* results)
{
	if (!results)
		return;
	free(results);
}

static BOOL rts_read_result(wStream* s, p_result_t* result, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(result);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 2, silent))
		return FALSE;
	Stream_Read_UINT16(s, result->result);
	Stream_Read_UINT16(s, result->reason);

	return rts_read_syntax_id(s, &result->transfer_syntax, silent);
}

static void rts_free_result(p_result_t* result)
{
	if (!result)
		return;
}

static BOOL rts_read_result_list(wStream* s, p_result_list_t* list, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(list);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 4, silent))
		return FALSE;
	Stream_Read_UINT8(s, list->n_results);  /* count */
	Stream_Read_UINT8(s, list->reserved);   /* alignment pad, m.b.z. */
	Stream_Read_UINT16(s, list->reserved2); /* alignment pad, m.b.z. */

	if (list->n_results > 0)
	{
		list->p_results = rts_result_new(list->n_results);
		if (!list->p_results)
			return FALSE;

		for (BYTE x = 0; x < list->n_results; x++)
		{
			p_result_t* result = &list->p_results[x]; /* size_is(n_results) */
			if (!rts_read_result(s, result, silent))
				return FALSE;
		}
	}

	return TRUE;
}

static void rts_free_result_list(p_result_list_t* list)
{
	if (!list)
		return;
	for (BYTE x = 0; x < list->n_results; x++)
	{
		p_result_t* result = &list->p_results[x];
		rts_free_result(result);
	}
	rts_result_free(list->p_results);
}

static void rts_free_pdu_alter_context(rpcconn_alter_context_hdr_t* ctx)
{
	if (!ctx)
		return;

	rts_free_context_list(&ctx->p_context_elem);
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_alter_context(wStream* s, rpcconn_alter_context_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_alter_context_hdr_t) - sizeof(rpcconn_common_hdr_t), silent))
		return FALSE;

	Stream_Read_UINT16(s, ctx->max_xmit_frag);
	Stream_Read_UINT16(s, ctx->max_recv_frag);
	Stream_Read_UINT32(s, ctx->assoc_group_id);

	if (!rts_read_context_list(s, &ctx->p_context_elem, silent))
		return FALSE;

	if (!rts_read_auth_verifier(s, &ctx->auth_verifier, &ctx->header, silent))
		return FALSE;

	return TRUE;
}

static BOOL rts_read_pdu_alter_context_response(wStream* s,
                                                rpcconn_alter_context_response_hdr_t* ctx,
                                                BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_alter_context_response_hdr_t) - sizeof(rpcconn_common_hdr_t),
	        silent))
		return FALSE;
	Stream_Read_UINT16(s, ctx->max_xmit_frag);
	Stream_Read_UINT16(s, ctx->max_recv_frag);
	Stream_Read_UINT32(s, ctx->assoc_group_id);

	if (!rts_read_port_any(s, &ctx->sec_addr, silent))
		return FALSE;

	if (!rts_align_stream(s, 4, silent))
		return FALSE;

	if (!rts_read_result_list(s, &ctx->p_result_list, silent))
		return FALSE;

	if (!rts_read_auth_verifier(s, &ctx->auth_verifier, &ctx->header, silent))
		return FALSE;

	return TRUE;
}

static void rts_free_pdu_alter_context_response(rpcconn_alter_context_response_hdr_t* ctx)
{
	if (!ctx)
		return;

	rts_free_port_any(&ctx->sec_addr);
	rts_free_result_list(&ctx->p_result_list);
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_bind(wStream* s, rpcconn_bind_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_bind_hdr_t) - sizeof(rpcconn_common_hdr_t), silent))
		return FALSE;
	Stream_Read_UINT16(s, ctx->max_xmit_frag);
	Stream_Read_UINT16(s, ctx->max_recv_frag);
	Stream_Read_UINT32(s, ctx->assoc_group_id);

	if (!rts_read_context_list(s, &ctx->p_context_elem, silent))
		return FALSE;

	if (!rts_read_auth_verifier(s, &ctx->auth_verifier, &ctx->header, silent))
		return FALSE;

	return TRUE;
}

static void rts_free_pdu_bind(rpcconn_bind_hdr_t* ctx)
{
	if (!ctx)
		return;
	rts_free_context_list(&ctx->p_context_elem);
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_bind_ack(wStream* s, rpcconn_bind_ack_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_CheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_bind_ack_hdr_t) - sizeof(rpcconn_common_hdr_t)))
		return FALSE;
	Stream_Read_UINT16(s, ctx->max_xmit_frag);
	Stream_Read_UINT16(s, ctx->max_recv_frag);
	Stream_Read_UINT32(s, ctx->assoc_group_id);

	if (!rts_read_port_any(s, &ctx->sec_addr, silent))
		return FALSE;

	if (!rts_align_stream(s, 4, silent))
		return FALSE;

	if (!rts_read_result_list(s, &ctx->p_result_list, silent))
		return FALSE;

	return rts_read_auth_verifier(s, &ctx->auth_verifier, &ctx->header, silent);
}

static void rts_free_pdu_bind_ack(rpcconn_bind_ack_hdr_t* ctx)
{
	if (!ctx)
		return;
	rts_free_port_any(&ctx->sec_addr);
	rts_free_result_list(&ctx->p_result_list);
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_bind_nak(wStream* s, rpcconn_bind_nak_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_bind_nak_hdr_t) - sizeof(rpcconn_common_hdr_t), silent))
		return FALSE;
	Stream_Read_UINT16(s, ctx->provider_reject_reason);
	return rts_read_supported_versions(s, &ctx->versions, silent);
}

static void rts_free_pdu_bind_nak(rpcconn_bind_nak_hdr_t* ctx)
{
	if (!ctx)
		return;

	rts_free_supported_versions(&ctx->versions);
}

static BOOL rts_read_pdu_auth3(wStream* s, rpcconn_rpc_auth_3_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_rpc_auth_3_hdr_t) - sizeof(rpcconn_common_hdr_t), silent))
		return FALSE;
	Stream_Read_UINT16(s, ctx->max_xmit_frag);
	Stream_Read_UINT16(s, ctx->max_recv_frag);

	return rts_read_auth_verifier(s, &ctx->auth_verifier, &ctx->header, silent);
}

static void rts_free_pdu_auth3(rpcconn_rpc_auth_3_hdr_t* ctx)
{
	if (!ctx)
		return;
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_fault(wStream* s, rpcconn_fault_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 12, silent))
		return FALSE;
	Stream_Read_UINT32(s, ctx->alloc_hint);
	Stream_Read_UINT16(s, ctx->p_cont_id);
	Stream_Read_UINT8(s, ctx->cancel_count);
	Stream_Read_UINT8(s, ctx->reserved);
	Stream_Read_UINT32(s, ctx->status);

	WLog_WARN(TAG, "status=%s", Win32ErrorCode2Tag(ctx->status & 0xFFFF));
	return rts_read_auth_verifier_with_stub(s, &ctx->auth_verifier, &ctx->header, silent);
}

static void rts_free_pdu_fault(rpcconn_fault_hdr_t* ctx)
{
	if (!ctx)
		return;
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_cancel_ack(wStream* s, rpcconn_cancel_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_cancel_hdr_t) - sizeof(rpcconn_common_hdr_t), silent))
		return FALSE;
	return rts_read_auth_verifier(s, &ctx->auth_verifier, &ctx->header, silent);
}

static void rts_free_pdu_cancel_ack(rpcconn_cancel_hdr_t* ctx)
{
	if (!ctx)
		return;
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_orphaned(wStream* s, rpcconn_orphaned_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_orphaned_hdr_t) - sizeof(rpcconn_common_hdr_t), silent))
		return FALSE;
	return rts_read_auth_verifier(s, &ctx->auth_verifier, &ctx->header, silent);
}

static void rts_free_pdu_orphaned(rpcconn_orphaned_hdr_t* ctx)
{
	if (!ctx)
		return;
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_request(wStream* s, rpcconn_request_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_request_hdr_t) - sizeof(rpcconn_common_hdr_t), silent))
		return FALSE;
	Stream_Read_UINT32(s, ctx->alloc_hint);
	Stream_Read_UINT16(s, ctx->p_cont_id);
	Stream_Read_UINT16(s, ctx->opnum);
	if (!rts_read_uuid(s, &ctx->object, silent))
		return FALSE;

	return rts_read_auth_verifier_with_stub(s, &ctx->auth_verifier, &ctx->header, silent);
}

static void rts_free_pdu_request(rpcconn_request_hdr_t* ctx)
{
	if (!ctx)
		return;
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_response(wStream* s, rpcconn_response_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_response_hdr_t) - sizeof(rpcconn_common_hdr_t), silent))
		return FALSE;
	Stream_Read_UINT32(s, ctx->alloc_hint);
	Stream_Read_UINT16(s, ctx->p_cont_id);
	Stream_Read_UINT8(s, ctx->cancel_count);
	Stream_Read_UINT8(s, ctx->reserved);

	if (!rts_align_stream(s, 8, silent))
		return FALSE;

	return rts_read_auth_verifier_with_stub(s, &ctx->auth_verifier, &ctx->header, silent);
}

static void rts_free_pdu_response(rpcconn_response_hdr_t* ctx)
{
	if (!ctx)
		return;
	free(ctx->stub_data);
	rts_free_auth_verifier(&ctx->auth_verifier);
}

static BOOL rts_read_pdu_rts(wStream* s, rpcconn_rts_hdr_t* ctx, BOOL silent)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(ctx);

	if (!Stream_ConditionalCheckAndLogRequiredLength(
	        TAG, s, sizeof(rpcconn_rts_hdr_t) - sizeof(rpcconn_common_hdr_t), silent))
		return FALSE;

	Stream_Read_UINT16(s, ctx->Flags);
	Stream_Read_UINT16(s, ctx->NumberOfCommands);
	return TRUE;
}

static void rts_free_pdu_rts(rpcconn_rts_hdr_t* ctx)
{
	WINPR_UNUSED(ctx);
}

void rts_free_pdu_header(rpcconn_hdr_t* header, BOOL allocated)
{
	if (!header)
		return;

	switch (header->common.ptype)
	{
		case PTYPE_ALTER_CONTEXT:
			rts_free_pdu_alter_context(&header->alter_context);
			break;
		case PTYPE_ALTER_CONTEXT_RESP:
			rts_free_pdu_alter_context_response(&header->alter_context_response);
			break;
		case PTYPE_BIND:
			rts_free_pdu_bind(&header->bind);
			break;
		case PTYPE_BIND_ACK:
			rts_free_pdu_bind_ack(&header->bind_ack);
			break;
		case PTYPE_BIND_NAK:
			rts_free_pdu_bind_nak(&header->bind_nak);
			break;
		case PTYPE_RPC_AUTH_3:
			rts_free_pdu_auth3(&header->rpc_auth_3);
			break;
		case PTYPE_CANCEL_ACK:
			rts_free_pdu_cancel_ack(&header->cancel);
			break;
		case PTYPE_FAULT:
			rts_free_pdu_fault(&header->fault);
			break;
		case PTYPE_ORPHANED:
			rts_free_pdu_orphaned(&header->orphaned);
			break;
		case PTYPE_REQUEST:
			rts_free_pdu_request(&header->request);
			break;
		case PTYPE_RESPONSE:
			rts_free_pdu_response(&header->response);
			break;
		case PTYPE_RTS:
			rts_free_pdu_rts(&header->rts);
			break;
			/* No extra fields */
		case PTYPE_SHUTDOWN:
			break;

		/* not handled */
		case PTYPE_PING:
		case PTYPE_WORKING:
		case PTYPE_NOCALL:
		case PTYPE_REJECT:
		case PTYPE_ACK:
		case PTYPE_CL_CANCEL:
		case PTYPE_FACK:
		case PTYPE_CO_CANCEL:
		default:
			break;
	}

	if (allocated)
		free(header);
}

BOOL rts_read_pdu_header(wStream* s, rpcconn_hdr_t* header)
{
	return rts_read_pdu_header_ex(s, header, FALSE);
}

BOOL rts_read_pdu_header_ex(wStream* s, rpcconn_hdr_t* header, BOOL silent)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	if (!rts_read_common_pdu_header(s, &header->common, silent))
		return FALSE;

	WLog_DBG(TAG, "Reading PDU type %s", rts_pdu_ptype_to_string(header->common.ptype));

	switch (header->common.ptype)
	{
		case PTYPE_ALTER_CONTEXT:
			rc = rts_read_pdu_alter_context(s, &header->alter_context, silent);
			break;
		case PTYPE_ALTER_CONTEXT_RESP:
			rc = rts_read_pdu_alter_context_response(s, &header->alter_context_response, silent);
			break;
		case PTYPE_BIND:
			rc = rts_read_pdu_bind(s, &header->bind, silent);
			break;
		case PTYPE_BIND_ACK:
			rc = rts_read_pdu_bind_ack(s, &header->bind_ack, silent);
			break;
		case PTYPE_BIND_NAK:
			rc = rts_read_pdu_bind_nak(s, &header->bind_nak, silent);
			break;
		case PTYPE_RPC_AUTH_3:
			rc = rts_read_pdu_auth3(s, &header->rpc_auth_3, silent);
			break;
		case PTYPE_CANCEL_ACK:
			rc = rts_read_pdu_cancel_ack(s, &header->cancel, silent);
			break;
		case PTYPE_FAULT:
			rc = rts_read_pdu_fault(s, &header->fault, silent);
			break;
		case PTYPE_ORPHANED:
			rc = rts_read_pdu_orphaned(s, &header->orphaned, silent);
			break;
		case PTYPE_REQUEST:
			rc = rts_read_pdu_request(s, &header->request, silent);
			break;
		case PTYPE_RESPONSE:
			rc = rts_read_pdu_response(s, &header->response, silent);
			break;
		case PTYPE_RTS:
			rc = rts_read_pdu_rts(s, &header->rts, silent);
			break;
		case PTYPE_SHUTDOWN:
			rc = TRUE; /* No extra fields */
			break;

		/* not handled */
		case PTYPE_PING:
		case PTYPE_WORKING:
		case PTYPE_NOCALL:
		case PTYPE_REJECT:
		case PTYPE_ACK:
		case PTYPE_CL_CANCEL:
		case PTYPE_FACK:
		case PTYPE_CO_CANCEL:
		default:
			break;
	}

	return rc;
}

static BOOL rts_write_pdu_header(wStream* s, const rpcconn_rts_hdr_t* header)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);
	if (!Stream_EnsureRemainingCapacity(s, sizeof(rpcconn_rts_hdr_t)))
		return FALSE;

	if (!rts_write_common_pdu_header(s, &header->header))
		return FALSE;

	Stream_Write_UINT16(s, header->Flags);
	Stream_Write_UINT16(s, header->NumberOfCommands);
	return TRUE;
}

/* [MS-RPCH] 2.2.3.5.1 ReceiveWindowSize */
static BOOL rts_receive_window_size_command_read(rdpRpc* rpc, wStream* buffer,
                                                 UINT32* ReceiveWindowSize)
{
	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);

	if (!Stream_CheckAndLogRequiredLength(TAG, buffer, 8))
		return FALSE;
	const uint32_t CommandType = Stream_Get_UINT32(buffer);
	if (CommandType != RTS_CMD_RECEIVE_WINDOW_SIZE)
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "[MS-RPCH] 2.2.3.5.1 ReceiveWindowSize::CommandType must be 0x08" PRIx32 ", got "
		           "0x%08" PRIx32,
		           RTS_CMD_RECEIVE_WINDOW_SIZE, CommandType);
		return FALSE;
	}
	const UINT32 val = Stream_Get_UINT32(buffer);
	if (ReceiveWindowSize)
		*ReceiveWindowSize = val; /* ReceiveWindowSize (4 bytes) */

	return TRUE;
}

/* [MS-RPCH] 2.2.3.5.1 ReceiveWindowSize */
static BOOL rts_receive_window_size_command_write(wStream* s, UINT32 ReceiveWindowSize)
{
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, 2 * sizeof(UINT32)))
		return FALSE;

	Stream_Write_UINT32(s, RTS_CMD_RECEIVE_WINDOW_SIZE); /* CommandType (4 bytes) */
	Stream_Write_UINT32(s, ReceiveWindowSize);           /* ReceiveWindowSize (4 bytes) */

	return TRUE;
}

/* [MS-RPCH] 2.2.3.5.2 FlowControlAck */
static int rts_flow_control_ack_command_read(rdpRpc* rpc, wStream* buffer, UINT32* BytesReceived,
                                             UINT32* AvailableWindow, BYTE* ChannelCookie)
{
	UINT32 val = 0;
	UINT32 Command = 0;

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);

	int rc = rts_destination_command_read(rpc, buffer, &Command);
	if (rc < 0)
		return rc;

	if (Command != RTS_CMD_FLOW_CONTROL_ACK)
	{
		char buffer1[64] = { 0 };
		char buffer2[64] = { 0 };
		WLog_Print(rpc->log, WLOG_ERROR, "got command %s, expected %s",
		           rts_command_to_string(Command, buffer1, sizeof(buffer1)),
		           rts_command_to_string(RTS_CMD_FLOW_CONTROL_ACK, buffer2, sizeof(buffer2)));
		return -1;
	}

	/* Ack (24 bytes) */
	if (!Stream_CheckAndLogRequiredLength(TAG, buffer, 24))
		return -1;

	Stream_Read_UINT32(buffer, val);
	if (BytesReceived)
		*BytesReceived = val; /* BytesReceived (4 bytes) */

	Stream_Read_UINT32(buffer, val);
	if (AvailableWindow)
		*AvailableWindow = val; /* AvailableWindow (4 bytes) */

	if (ChannelCookie)
		Stream_Read(buffer, ChannelCookie, 16); /* ChannelCookie (16 bytes) */
	else
		Stream_Seek(buffer, 16);
	return 24;
}

/* [MS-RPCH] 2.2.3.5.2 FlowControlAck */
static BOOL rts_flow_control_ack_command_write(wStream* s, UINT32 BytesReceived,
                                               UINT32 AvailableWindow, BYTE* ChannelCookie)
{
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, 28))
		return FALSE;

	Stream_Write_UINT32(s, RTS_CMD_FLOW_CONTROL_ACK); /* CommandType (4 bytes) */
	Stream_Write_UINT32(s, BytesReceived);            /* BytesReceived (4 bytes) */
	Stream_Write_UINT32(s, AvailableWindow);          /* AvailableWindow (4 bytes) */
	Stream_Write(s, ChannelCookie, 16);               /* ChannelCookie (16 bytes) */

	return TRUE;
}

/* [MS-RPCH] 2.2.3.5.3 ConnectionTimeout */
static BOOL rts_connection_timeout_command_read(WINPR_ATTR_UNUSED rdpRpc* rpc, wStream* buffer,
                                                UINT32* ConnectionTimeout)
{
	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);

	if (!Stream_CheckAndLogRequiredLength(TAG, buffer, 8))
		return FALSE;

	const uint32_t CommandType = Stream_Get_UINT32(buffer);
	if (CommandType != RTS_CMD_CONNECTION_TIMEOUT)
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "[MS-RPCH] 2.2.3.5.3 ConnectionTimeout::CommandType must be 0x08" PRIx32 ", got "
		           "0x%08" PRIx32,
		           RTS_CMD_CONNECTION_TIMEOUT, CommandType);
		return FALSE;
	}
	const UINT32 val = Stream_Get_UINT32(buffer);
	if (ConnectionTimeout)
		*ConnectionTimeout = val; /* ConnectionTimeout (4 bytes) */

	return TRUE;
}

static BOOL rts_cookie_command_write(wStream* s, const BYTE* Cookie)
{
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, 20))
		return FALSE;

	Stream_Write_UINT32(s, RTS_CMD_COOKIE); /* CommandType (4 bytes) */
	Stream_Write(s, Cookie, 16);            /* Cookie (16 bytes) */

	return TRUE;
}

static BOOL rts_channel_lifetime_command_write(wStream* s, UINT32 ChannelLifetime)
{
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;
	Stream_Write_UINT32(s, RTS_CMD_CHANNEL_LIFETIME); /* CommandType (4 bytes) */
	Stream_Write_UINT32(s, ChannelLifetime);          /* ChannelLifetime (4 bytes) */

	return TRUE;
}

static BOOL rts_client_keepalive_command_write(wStream* s, UINT32 ClientKeepalive)
{
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;
	/**
	 * An unsigned integer that specifies the keep-alive interval, in milliseconds,
	 * that this connection is configured to use. This value MUST be 0 or in the inclusive
	 * range of 60,000 through 4,294,967,295. If it is 0, it MUST be interpreted as 300,000.
	 */

	Stream_Write_UINT32(s, RTS_CMD_CLIENT_KEEPALIVE); /* CommandType (4 bytes) */
	Stream_Write_UINT32(s, ClientKeepalive);          /* ClientKeepalive (4 bytes) */

	return TRUE;
}

/* [MS-RPCH] 2.2.3.5.7 Version */
static BOOL rts_version_command_read(rdpRpc* rpc, wStream* buffer, uint32_t* pversion)
{
	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);

	if (!Stream_EnsureRemainingCapacity(buffer, 8))
		return FALSE;

	const uint32_t CommandType = Stream_Get_UINT32(buffer); /* CommandType (4 bytes) */
	if (CommandType != RTS_CMD_VERSION)
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "[MS-RPCH] 2.2.3.5.7 Version::CommandType must be 0x08" PRIx32 ", got "
		           "0x%08" PRIx32,
		           RTS_CMD_VERSION, CommandType);
		return FALSE;
	}
	const uint32_t version = Stream_Get_UINT32(buffer); /* Version (4 bytes) */
	if (version != 1)
	{
		WLog_Print(rpc->log, WLOG_WARN,
		           "[MS-RPCH] 2.2.3.5.7 Version::Version should be 0x00000001, got 0x%08" PRIx32,
		           version);
	}
	if (pversion)
		*pversion = version;

	return TRUE;
}

/* [MS-RPCH] 2.2.3.5.7 Version */
static BOOL rts_version_command_write(wStream* buffer)
{
	WINPR_ASSERT(buffer);

	if (!Stream_EnsureRemainingCapacity((buffer), 8))
		return FALSE;

	Stream_Write_UINT32(buffer, RTS_CMD_VERSION); /* CommandType (4 bytes) */
	Stream_Write_UINT32(buffer, 1);               /* Version (4 bytes) */

	return TRUE;
}

static BOOL rts_empty_command_write(wStream* s)
{
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT32(s, RTS_CMD_EMPTY); /* CommandType (4 bytes) */

	return TRUE;
}

static BOOL rts_padding_command_read(wStream* s, size_t* length, BOOL silent)
{
	UINT32 ConformanceCount = 0;
	WINPR_ASSERT(s);
	WINPR_ASSERT(length);
	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 4, silent))
		return FALSE;
	Stream_Read_UINT32(s, ConformanceCount); /* ConformanceCount (4 bytes) */
	*length = ConformanceCount + 4;
	return TRUE;
}

static BOOL rts_client_address_command_read(wStream* s, size_t* length, BOOL silent)
{
	UINT32 AddressType = 0;

	WINPR_ASSERT(s);
	WINPR_ASSERT(length);

	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, 4, silent))
		return FALSE;
	Stream_Read_UINT32(s, AddressType); /* AddressType (4 bytes) */

	if (AddressType == 0)
	{
		/* ClientAddress (4 bytes) */
		/* padding (12 bytes) */
		*length = 4 + 4 + 12;
	}
	else
	{
		/* ClientAddress (16 bytes) */
		/* padding (12 bytes) */
		*length = 4 + 16 + 12;
	}
	return TRUE;
}

static BOOL rts_association_group_id_command_write(wStream* s, const BYTE* AssociationGroupId)
{
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, 20))
		return FALSE;

	Stream_Write_UINT32(s, RTS_CMD_ASSOCIATION_GROUP_ID); /* CommandType (4 bytes) */
	Stream_Write(s, AssociationGroupId, 16);              /* AssociationGroupId (16 bytes) */

	return TRUE;
}

static int rts_destination_command_read(WINPR_ATTR_UNUSED rdpRpc* rpc, wStream* buffer,
                                        UINT32* Destination)
{
	UINT32 val = 0;
	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);

	if (!Stream_CheckAndLogRequiredLength(TAG, buffer, 4))
		return -1;
	Stream_Read_UINT32(buffer, val);
	if (Destination)
		*Destination = val; /* Destination (4 bytes) */

	return 4;
}

static BOOL rts_destination_command_write(wStream* s, UINT32 Destination)
{
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT32(s, RTS_CMD_DESTINATION); /* CommandType (4 bytes) */
	Stream_Write_UINT32(s, Destination);         /* Destination (4 bytes) */

	return TRUE;
}

void rts_generate_cookie(BYTE* cookie)
{
	WINPR_ASSERT(cookie);
	winpr_RAND(cookie, 16);
}

#define rts_send_buffer(channel, s, frag_length) \
	rts_send_buffer_int((channel), (s), (frag_length), __FILE__, __LINE__, __func__)
static BOOL rts_send_buffer_int(RpcChannel* channel, wStream* s, size_t frag_length,
                                const char* file, size_t line, const char* fkt)
{
	BOOL status = FALSE;
	SSIZE_T rc = 0;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->rpc);
	WINPR_ASSERT(s);

	Stream_SealLength(s);

	const DWORD level = WLOG_TRACE;
	if (WLog_IsLevelActive(channel->rpc->log, level))
	{
		WLog_PrintMessage(channel->rpc->log, WLOG_MESSAGE_TEXT, level, line, file, fkt,
		                  "Sending [%s] %" PRIuz " bytes", fkt, Stream_Length(s));
	}
	if (Stream_Length(s) < sizeof(rpcconn_common_hdr_t))
		goto fail;
	if (Stream_Length(s) != frag_length)
		goto fail;

	rc = rpc_channel_write(channel, Stream_Buffer(s), Stream_Length(s));
	if (rc < 0)
		goto fail;
	if ((size_t)rc != Stream_Length(s))
		goto fail;
	status = TRUE;
fail:
	return status;
}

/* CONN/A Sequence */

BOOL rts_send_CONN_A1_pdu(rdpRpc* rpc)
{
	BOOL status = FALSE;
	wStream* buffer = NULL;
	rpcconn_rts_hdr_t header = rts_pdu_header_init();
	UINT32 ReceiveWindowSize = 0;
	BYTE* OUTChannelCookie = NULL;
	BYTE* VirtualConnectionCookie = NULL;
	RpcVirtualConnection* connection = NULL;
	RpcOutChannel* outChannel = NULL;

	WINPR_ASSERT(rpc);

	connection = rpc->VirtualConnection;
	WINPR_ASSERT(connection);

	outChannel = connection->DefaultOutChannel;
	WINPR_ASSERT(outChannel);

	header.header.frag_length = 76;
	header.Flags = RTS_FLAG_NONE;
	header.NumberOfCommands = 4;

	WLog_DBG(TAG, "Sending CONN/A1 RTS PDU");
	VirtualConnectionCookie = (BYTE*)&(connection->Cookie);
	OUTChannelCookie = (BYTE*)&(outChannel->common.Cookie);
	ReceiveWindowSize = outChannel->ReceiveWindow;

	buffer = Stream_New(NULL, header.header.frag_length);

	if (!buffer)
		return -1;

	if (!rts_write_pdu_header(buffer, &header)) /* RTS Header (20 bytes) */
		goto fail;
	status = rts_version_command_write(buffer); /* Version (8 bytes) */
	if (!status)
		goto fail;
	status = rts_cookie_command_write(
	    buffer, VirtualConnectionCookie); /* VirtualConnectionCookie (20 bytes) */
	if (!status)
		goto fail;
	status = rts_cookie_command_write(buffer, OUTChannelCookie); /* OUTChannelCookie (20 bytes) */
	if (!status)
		goto fail;
	status = rts_receive_window_size_command_write(
	    buffer, ReceiveWindowSize); /* ReceiveWindowSize (8 bytes) */
	if (!status)
		goto fail;
	status = rts_send_buffer(&outChannel->common, buffer, header.header.frag_length);
fail:
	Stream_Free(buffer, TRUE);
	return status;
}

BOOL rts_recv_CONN_A3_pdu(rdpRpc* rpc, wStream* buffer)
{
	BOOL rc = FALSE;
	UINT32 ConnectionTimeout = 0;

	rpcconn_hdr_t header = { 0 };
	if (!rts_read_pdu_header(buffer, &header))
		goto fail;

	if (header.rts.Flags != RTS_FLAG_NONE)
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "[MS-RPCH] 2.2.4.4 CONN/A3 RTS PDU unexpected Flags=0x%08" PRIx32
		           ", expected 0x%08" PRIx32,
		           header.rts.Flags, RTS_FLAG_NONE);
		goto fail;
	}
	if (header.rts.NumberOfCommands != 1)
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "[MS-RPCH] 2.2.4.4 CONN/A3 RTS PDU unexpected NumberOfCommands=%" PRIu32
		           ", expected 1",
		           header.rts.NumberOfCommands);
		goto fail;
	}

	if (!rts_connection_timeout_command_read(rpc, buffer, &ConnectionTimeout))
		goto fail;

	WLog_Print(rpc->log, WLOG_DEBUG, "Receiving CONN/A3 RTS PDU: ConnectionTimeout: %" PRIu32 "",
	           ConnectionTimeout);

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(rpc->VirtualConnection);
	WINPR_ASSERT(rpc->VirtualConnection->DefaultInChannel);

	rpc->VirtualConnection->DefaultInChannel->PingOriginator.ConnectionTimeout = ConnectionTimeout;

	rc = TRUE;

fail:
	rts_free_pdu_header(&header, FALSE);
	return rc;
}

/* CONN/B Sequence */

BOOL rts_send_CONN_B1_pdu(rdpRpc* rpc)
{
	BOOL status = FALSE;
	wStream* buffer = NULL;
	rpcconn_rts_hdr_t header = rts_pdu_header_init();
	BYTE* INChannelCookie = NULL;
	BYTE* AssociationGroupId = NULL;
	BYTE* VirtualConnectionCookie = NULL;
	RpcVirtualConnection* connection = NULL;
	RpcInChannel* inChannel = NULL;

	WINPR_ASSERT(rpc);

	connection = rpc->VirtualConnection;
	WINPR_ASSERT(connection);

	inChannel = connection->DefaultInChannel;
	WINPR_ASSERT(inChannel);

	header.header.frag_length = 104;
	header.Flags = RTS_FLAG_NONE;
	header.NumberOfCommands = 6;

	WLog_DBG(TAG, "Sending CONN/B1 RTS PDU");

	VirtualConnectionCookie = (BYTE*)&(connection->Cookie);
	INChannelCookie = (BYTE*)&(inChannel->common.Cookie);
	AssociationGroupId = (BYTE*)&(connection->AssociationGroupId);
	buffer = Stream_New(NULL, header.header.frag_length);

	if (!buffer)
		goto fail;
	if (!rts_write_pdu_header(buffer, &header)) /* RTS Header (20 bytes) */
		goto fail;
	if (!rts_version_command_write(buffer)) /* Version (8 bytes) */
		goto fail;
	if (!rts_cookie_command_write(buffer,
	                              VirtualConnectionCookie)) /* VirtualConnectionCookie (20 bytes) */
		goto fail;
	if (!rts_cookie_command_write(buffer, INChannelCookie)) /* INChannelCookie (20 bytes) */
		goto fail;
	if (!rts_channel_lifetime_command_write(buffer,
	                                        rpc->ChannelLifetime)) /* ChannelLifetime (8 bytes) */
		goto fail;
	if (!rts_client_keepalive_command_write(buffer,
	                                        rpc->KeepAliveInterval)) /* ClientKeepalive (8 bytes) */
		goto fail;
	if (!rts_association_group_id_command_write(
	        buffer, AssociationGroupId)) /* AssociationGroupId (20 bytes) */
		goto fail;
	status = rts_send_buffer(&inChannel->common, buffer, header.header.frag_length);
fail:
	Stream_Free(buffer, TRUE);
	return status;
}

/* [MS-RPCH] 2.2.4.9 CONN/C2 RTS PDU */

BOOL rts_recv_CONN_C2_pdu(rdpRpc* rpc, wStream* buffer)
{
	BOOL rc = FALSE;
	UINT32 ReceiveWindowSize = 0;
	UINT32 ConnectionTimeout = 0;

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);

	rpcconn_hdr_t header = { 0 };
	if (!rts_read_pdu_header(buffer, &header))
		goto fail;

	if (header.rts.Flags != RTS_FLAG_NONE)
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "[MS-RPCH] 2.2.4.9 CONN/C2 RTS PDU unexpected Flags=0x%08" PRIx32
		           ", expected 0x%08" PRIx32,
		           header.rts.Flags, RTS_FLAG_NONE);
		goto fail;
	}
	if (header.rts.NumberOfCommands != 3)
	{
		WLog_Print(rpc->log, WLOG_ERROR,
		           "[MS-RPCH] 2.2.4.9 CONN/C2 RTS PDU unexpected NumberOfCommands=%" PRIu32
		           ", expected 3",
		           header.rts.NumberOfCommands);
		goto fail;
	}
	if (!rts_version_command_read(rpc, buffer, NULL))
		goto fail;

	if (!rts_receive_window_size_command_read(rpc, buffer, &ReceiveWindowSize))
		goto fail;

	if (!rts_connection_timeout_command_read(rpc, buffer, &ConnectionTimeout))
		goto fail;

	WLog_Print(rpc->log, WLOG_DEBUG,
	           "Receiving CONN/C2 RTS PDU: ConnectionTimeout: %" PRIu32
	           " ReceiveWindowSize: %" PRIu32 "",
	           ConnectionTimeout, ReceiveWindowSize);

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(rpc->VirtualConnection);
	WINPR_ASSERT(rpc->VirtualConnection->DefaultInChannel);

	rpc->VirtualConnection->DefaultInChannel->PingOriginator.ConnectionTimeout = ConnectionTimeout;
	rpc->VirtualConnection->DefaultInChannel->PeerReceiveWindow = ReceiveWindowSize;

	rc = TRUE;

fail:
	rts_free_pdu_header(&header, FALSE);
	return rc;
}

/* Out-of-Sequence PDUs */

BOOL rts_send_flow_control_ack_pdu(rdpRpc* rpc)
{
	BOOL status = FALSE;
	wStream* buffer = NULL;
	rpcconn_rts_hdr_t header = rts_pdu_header_init();
	UINT32 BytesReceived = 0;
	UINT32 AvailableWindow = 0;
	BYTE* ChannelCookie = NULL;
	RpcVirtualConnection* connection = NULL;
	RpcInChannel* inChannel = NULL;
	RpcOutChannel* outChannel = NULL;

	WINPR_ASSERT(rpc);

	connection = rpc->VirtualConnection;
	WINPR_ASSERT(connection);

	inChannel = connection->DefaultInChannel;
	WINPR_ASSERT(inChannel);

	outChannel = connection->DefaultOutChannel;
	WINPR_ASSERT(outChannel);

	header.header.frag_length = 56;
	header.Flags = RTS_FLAG_OTHER_CMD;
	header.NumberOfCommands = 2;

	WLog_DBG(TAG, "Sending FlowControlAck RTS PDU");

	BytesReceived = outChannel->BytesReceived;
	AvailableWindow = outChannel->AvailableWindowAdvertised;
	ChannelCookie = (BYTE*)&(outChannel->common.Cookie);
	outChannel->ReceiverAvailableWindow = outChannel->AvailableWindowAdvertised;
	buffer = Stream_New(NULL, header.header.frag_length);

	if (!buffer)
		goto fail;

	if (!rts_write_pdu_header(buffer, &header)) /* RTS Header (20 bytes) */
		goto fail;
	if (!rts_destination_command_write(buffer, FDOutProxy)) /* Destination Command (8 bytes) */
		goto fail;

	/* FlowControlAck Command (28 bytes) */
	if (!rts_flow_control_ack_command_write(buffer, BytesReceived, AvailableWindow, ChannelCookie))
		goto fail;

	status = rts_send_buffer(&inChannel->common, buffer, header.header.frag_length);
fail:
	Stream_Free(buffer, TRUE);
	return status;
}

static int rts_recv_flow_control_ack_pdu(rdpRpc* rpc, wStream* buffer)
{
	int rc = 0;
	UINT32 BytesReceived = 0;
	UINT32 AvailableWindow = 0;
	BYTE ChannelCookie[16] = { 0 };

	rc = rts_flow_control_ack_command_read(rpc, buffer, &BytesReceived, &AvailableWindow,
	                                       (BYTE*)&ChannelCookie);
	if (rc < 0)
		return rc;
	WLog_ERR(TAG,
	         "Receiving FlowControlAck RTS PDU: BytesReceived: %" PRIu32
	         " AvailableWindow: %" PRIu32 "",
	         BytesReceived, AvailableWindow);

	WINPR_ASSERT(rpc->VirtualConnection);
	WINPR_ASSERT(rpc->VirtualConnection->DefaultInChannel);

	rpc->VirtualConnection->DefaultInChannel->SenderAvailableWindow =
	    AvailableWindow - (rpc->VirtualConnection->DefaultInChannel->BytesSent - BytesReceived);
	return 1;
}

static int rts_recv_flow_control_ack_with_destination_pdu(rdpRpc* rpc, wStream* buffer)
{
	UINT32 Command = 0;
	UINT32 Destination = 0;
	UINT32 BytesReceived = 0;
	UINT32 AvailableWindow = 0;
	BYTE ChannelCookie[16] = { 0 };
	/**
	 * When the sender receives a FlowControlAck RTS PDU, it MUST use the following formula to
	 * recalculate its Sender AvailableWindow variable:
	 *
	 * Sender AvailableWindow = Receiver AvailableWindow_from_ack - (BytesSent -
	 * BytesReceived_from_ack)
	 *
	 * Where:
	 *
	 * Receiver AvailableWindow_from_ack is the Available Window field in the Flow Control
	 * Acknowledgement Structure (section 2.2.3.4) in the PDU received.
	 *
	 * BytesReceived_from_ack is the Bytes Received field in the Flow Control Acknowledgement
	 * structure in the PDU received.
	 *
	 */

	int rc = rts_destination_command_read(rpc, buffer, &Command);
	if (rc < 0)
		return rc;

	if (Command != RTS_CMD_DESTINATION)
	{
		char buffer1[64] = { 0 };
		char buffer2[64] = { 0 };
		WLog_Print(rpc->log, WLOG_ERROR, "got command %s, expected %s",
		           rts_command_to_string(Command, buffer1, sizeof(buffer1)),
		           rts_command_to_string(RTS_CMD_DESTINATION, buffer2, sizeof(buffer2)));
		return -1;
	}

	rc = rts_destination_command_read(rpc, buffer, &Destination);
	if (rc < 0)
		return rc;

	switch (Destination)
	{
		case FDClient:
			break;
		case FDInProxy:
			break;
		case FDServer:
			break;
		case FDOutProxy:
			break;
		default:
			WLog_Print(rpc->log, WLOG_ERROR,
			           "got destination %" PRIu32
			           ", expected one of [FDClient[0]|FDInProxy[1]|FDServer[2]|FDOutProxy[3]",
			           Destination);
			return -1;
	}

	rc = rts_flow_control_ack_command_read(rpc, buffer, &BytesReceived, &AvailableWindow,
	                                       ChannelCookie);
	if (rc < 0)
		return rc;

	WLog_DBG(TAG,
	         "Receiving FlowControlAckWithDestination RTS PDU: BytesReceived: %" PRIu32
	         " AvailableWindow: %" PRIu32 "",
	         BytesReceived, AvailableWindow);

	WINPR_ASSERT(rpc->VirtualConnection);
	WINPR_ASSERT(rpc->VirtualConnection->DefaultInChannel);
	rpc->VirtualConnection->DefaultInChannel->SenderAvailableWindow =
	    AvailableWindow - (rpc->VirtualConnection->DefaultInChannel->BytesSent - BytesReceived);
	return 1;
}

BOOL rts_recv_ping_pdu(rdpRpc* rpc, wStream* s)
{
	BOOL rc = FALSE;
	rpcconn_hdr_t header = { 0 };

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(rpc->auth);
	WINPR_ASSERT(s);

	if (!rts_read_pdu_header(s, &header))
		goto fail;

	rc = TRUE;
	if (header.common.ptype != PTYPE_RTS)
	{
		WLog_Print(rpc->log, WLOG_ERROR, "received invalid ping PDU, type is 0x%" PRIx32,
		           header.common.ptype);
		rc = FALSE;
	}
	if (header.rts.Flags != RTS_FLAG_PING)
	{
		WLog_Print(rpc->log, WLOG_ERROR, "received unexpected ping PDU::Flags 0x%" PRIx32,
		           header.rts.Flags);
		rc = FALSE;
	}
fail:
	rts_free_pdu_header(&header, FALSE);
	return rc;
}

static int rts_send_ping_pdu(rdpRpc* rpc)
{
	BOOL status = FALSE;
	wStream* buffer = NULL;
	rpcconn_rts_hdr_t header = rts_pdu_header_init();
	RpcInChannel* inChannel = NULL;

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(rpc->VirtualConnection);

	inChannel = rpc->VirtualConnection->DefaultInChannel;
	WINPR_ASSERT(inChannel);

	header.header.frag_length = 20;
	header.Flags = RTS_FLAG_PING;
	header.NumberOfCommands = 0;

	WLog_DBG(TAG, "Sending Ping RTS PDU");
	buffer = Stream_New(NULL, header.header.frag_length);

	if (!buffer)
		goto fail;

	if (!rts_write_pdu_header(buffer, &header)) /* RTS Header (20 bytes) */
		goto fail;
	status = rts_send_buffer(&inChannel->common, buffer, header.header.frag_length);
fail:
	Stream_Free(buffer, TRUE);
	return (status) ? 1 : -1;
}

BOOL rts_command_length(UINT32 CommandType, wStream* s, size_t* length, BOOL silent)
{
	size_t padding = 0;
	size_t CommandLength = 0;

	WINPR_ASSERT(s);

	switch (CommandType)
	{
		case RTS_CMD_RECEIVE_WINDOW_SIZE:
			CommandLength = RTS_CMD_RECEIVE_WINDOW_SIZE_LENGTH;
			break;

		case RTS_CMD_FLOW_CONTROL_ACK:
			CommandLength = RTS_CMD_FLOW_CONTROL_ACK_LENGTH;
			break;

		case RTS_CMD_CONNECTION_TIMEOUT:
			CommandLength = RTS_CMD_CONNECTION_TIMEOUT_LENGTH;
			break;

		case RTS_CMD_COOKIE:
			CommandLength = RTS_CMD_COOKIE_LENGTH;
			break;

		case RTS_CMD_CHANNEL_LIFETIME:
			CommandLength = RTS_CMD_CHANNEL_LIFETIME_LENGTH;
			break;

		case RTS_CMD_CLIENT_KEEPALIVE:
			CommandLength = RTS_CMD_CLIENT_KEEPALIVE_LENGTH;
			break;

		case RTS_CMD_VERSION:
			CommandLength = RTS_CMD_VERSION_LENGTH;
			break;

		case RTS_CMD_EMPTY:
			CommandLength = RTS_CMD_EMPTY_LENGTH;
			break;

		case RTS_CMD_PADDING: /* variable-size */
			if (!rts_padding_command_read(s, &padding, silent))
				return FALSE;
			break;

		case RTS_CMD_NEGATIVE_ANCE:
			CommandLength = RTS_CMD_NEGATIVE_ANCE_LENGTH;
			break;

		case RTS_CMD_ANCE:
			CommandLength = RTS_CMD_ANCE_LENGTH;
			break;

		case RTS_CMD_CLIENT_ADDRESS: /* variable-size */
			if (!rts_client_address_command_read(s, &CommandLength, silent))
				return FALSE;
			break;

		case RTS_CMD_ASSOCIATION_GROUP_ID:
			CommandLength = RTS_CMD_ASSOCIATION_GROUP_ID_LENGTH;
			break;

		case RTS_CMD_DESTINATION:
			CommandLength = RTS_CMD_DESTINATION_LENGTH;
			break;

		case RTS_CMD_PING_TRAFFIC_SENT_NOTIFY:
			CommandLength = RTS_CMD_PING_TRAFFIC_SENT_NOTIFY_LENGTH;
			break;

		default:
			WLog_ERR(TAG, "Error: Unknown RTS Command Type: 0x%" PRIx32 "", CommandType);
			return FALSE;
	}

	CommandLength += padding;
	if (!Stream_ConditionalCheckAndLogRequiredLength(TAG, s, CommandLength, silent))
		return FALSE;

	if (length)
		*length = CommandLength;
	return TRUE;
}

static int rts_send_OUT_R2_A7_pdu(rdpRpc* rpc)
{
	BOOL status = FALSE;
	wStream* buffer = NULL;
	rpcconn_rts_hdr_t header = rts_pdu_header_init();
	BYTE* SuccessorChannelCookie = NULL;
	RpcInChannel* inChannel = NULL;
	RpcOutChannel* nextOutChannel = NULL;

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(rpc->VirtualConnection);

	inChannel = rpc->VirtualConnection->DefaultInChannel;
	WINPR_ASSERT(inChannel);

	nextOutChannel = rpc->VirtualConnection->NonDefaultOutChannel;
	WINPR_ASSERT(nextOutChannel);

	header.header.frag_length = 56;
	header.Flags = RTS_FLAG_OUT_CHANNEL;
	header.NumberOfCommands = 3;

	WLog_DBG(TAG, "Sending OUT_R2/A7 RTS PDU");

	SuccessorChannelCookie = (BYTE*)&(nextOutChannel->common.Cookie);
	buffer = Stream_New(NULL, header.header.frag_length);

	if (!buffer)
		return -1;

	if (!rts_write_pdu_header(buffer, &header)) /* RTS Header (20 bytes) */
		goto fail;
	if (!rts_destination_command_write(buffer, FDServer)) /* Destination (8 bytes)*/
		goto fail;
	if (!rts_cookie_command_write(buffer,
	                              SuccessorChannelCookie)) /* SuccessorChannelCookie (20 bytes) */
		goto fail;
	if (!rts_version_command_write(buffer)) /* Version (8 bytes) */
		goto fail;
	status = rts_send_buffer(&inChannel->common, buffer, header.header.frag_length);
fail:
	Stream_Free(buffer, TRUE);
	return (status) ? 1 : -1;
}

static int rts_send_OUT_R2_C1_pdu(rdpRpc* rpc)
{
	BOOL status = FALSE;
	wStream* buffer = NULL;
	rpcconn_rts_hdr_t header = rts_pdu_header_init();
	RpcOutChannel* nextOutChannel = NULL;

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(rpc->VirtualConnection);

	nextOutChannel = rpc->VirtualConnection->NonDefaultOutChannel;
	WINPR_ASSERT(nextOutChannel);

	header.header.frag_length = 24;
	header.Flags = RTS_FLAG_PING;
	header.NumberOfCommands = 1;

	WLog_DBG(TAG, "Sending OUT_R2/C1 RTS PDU");
	buffer = Stream_New(NULL, header.header.frag_length);

	if (!buffer)
		return -1;

	if (!rts_write_pdu_header(buffer, &header)) /* RTS Header (20 bytes) */
		goto fail;

	if (!rts_empty_command_write(buffer)) /* Empty command (4 bytes) */
		goto fail;
	status = rts_send_buffer(&nextOutChannel->common, buffer, header.header.frag_length);
fail:
	Stream_Free(buffer, TRUE);
	return (status) ? 1 : -1;
}

BOOL rts_send_OUT_R1_A3_pdu(rdpRpc* rpc)
{
	BOOL status = FALSE;
	wStream* buffer = NULL;
	rpcconn_rts_hdr_t header = rts_pdu_header_init();
	UINT32 ReceiveWindowSize = 0;
	BYTE* VirtualConnectionCookie = NULL;
	BYTE* PredecessorChannelCookie = NULL;
	BYTE* SuccessorChannelCookie = NULL;
	RpcVirtualConnection* connection = NULL;
	RpcOutChannel* outChannel = NULL;
	RpcOutChannel* nextOutChannel = NULL;

	WINPR_ASSERT(rpc);

	connection = rpc->VirtualConnection;
	WINPR_ASSERT(connection);

	outChannel = connection->DefaultOutChannel;
	WINPR_ASSERT(outChannel);

	nextOutChannel = connection->NonDefaultOutChannel;
	WINPR_ASSERT(nextOutChannel);

	header.header.frag_length = 96;
	header.Flags = RTS_FLAG_RECYCLE_CHANNEL;
	header.NumberOfCommands = 5;

	WLog_DBG(TAG, "Sending OUT_R1/A3 RTS PDU");

	VirtualConnectionCookie = (BYTE*)&(connection->Cookie);
	PredecessorChannelCookie = (BYTE*)&(outChannel->common.Cookie);
	SuccessorChannelCookie = (BYTE*)&(nextOutChannel->common.Cookie);
	ReceiveWindowSize = outChannel->ReceiveWindow;
	buffer = Stream_New(NULL, header.header.frag_length);

	if (!buffer)
		return -1;

	if (!rts_write_pdu_header(buffer, &header)) /* RTS Header (20 bytes) */
		goto fail;
	if (!rts_version_command_write(buffer)) /* Version (8 bytes) */
		goto fail;
	if (!rts_cookie_command_write(buffer,
	                              VirtualConnectionCookie)) /* VirtualConnectionCookie (20 bytes) */
		goto fail;
	if (!rts_cookie_command_write(
	        buffer, PredecessorChannelCookie)) /* PredecessorChannelCookie (20 bytes) */
		goto fail;
	if (!rts_cookie_command_write(buffer,
	                              SuccessorChannelCookie)) /* SuccessorChannelCookie (20 bytes) */
		goto fail;
	if (!rts_receive_window_size_command_write(buffer,
	                                           ReceiveWindowSize)) /* ReceiveWindowSize (8 bytes) */
		goto fail;

	status = rts_send_buffer(&nextOutChannel->common, buffer, header.header.frag_length);
fail:
	Stream_Free(buffer, TRUE);
	return status;
}

static int rts_recv_OUT_R1_A2_pdu(rdpRpc* rpc, wStream* buffer)
{
	int status = 0;
	UINT32 Destination = 0;
	RpcVirtualConnection* connection = NULL;
	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);

	connection = rpc->VirtualConnection;
	WINPR_ASSERT(connection);

	WLog_DBG(TAG, "Receiving OUT R1/A2 RTS PDU");

	status = rts_destination_command_read(rpc, buffer, &Destination);
	if (status < 0)
		return status;

	connection->NonDefaultOutChannel = rpc_out_channel_new(rpc, &connection->Cookie);

	if (!connection->NonDefaultOutChannel)
		return -1;

	status = rpc_out_channel_replacement_connect(connection->NonDefaultOutChannel, 5000);

	if (status < 0)
	{
		WLog_ERR(TAG, "rpc_out_channel_replacement_connect failure");
		return -1;
	}

	rpc_out_channel_transition_to_state(connection->DefaultOutChannel,
	                                    CLIENT_OUT_CHANNEL_STATE_OPENED_A6W);
	return 1;
}

static int rts_recv_OUT_R2_A6_pdu(rdpRpc* rpc, WINPR_ATTR_UNUSED wStream* buffer)
{
	int status = 0;
	RpcVirtualConnection* connection = NULL;

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);

	connection = rpc->VirtualConnection;
	WINPR_ASSERT(connection);

	WLog_DBG(TAG, "Receiving OUT R2/A6 RTS PDU");
	status = rts_send_OUT_R2_C1_pdu(rpc);

	if (status < 0)
	{
		WLog_ERR(TAG, "rts_send_OUT_R2_C1_pdu failure");
		return -1;
	}

	status = rts_send_OUT_R2_A7_pdu(rpc);

	if (status < 0)
	{
		WLog_ERR(TAG, "rts_send_OUT_R2_A7_pdu failure");
		return -1;
	}

	rpc_out_channel_transition_to_state(connection->NonDefaultOutChannel,
	                                    CLIENT_OUT_CHANNEL_STATE_OPENED_B3W);
	rpc_out_channel_transition_to_state(connection->DefaultOutChannel,
	                                    CLIENT_OUT_CHANNEL_STATE_OPENED_B3W);
	return 1;
}

static int rts_recv_OUT_R2_B3_pdu(rdpRpc* rpc, WINPR_ATTR_UNUSED wStream* buffer)
{
	RpcVirtualConnection* connection = NULL;

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);

	connection = rpc->VirtualConnection;
	WINPR_ASSERT(connection);

	WLog_DBG(TAG, "Receiving OUT R2/B3 RTS PDU");
	rpc_out_channel_transition_to_state(connection->DefaultOutChannel,
	                                    CLIENT_OUT_CHANNEL_STATE_RECYCLED);
	return 1;
}

BOOL rts_recv_out_of_sequence_pdu(rdpRpc* rpc, wStream* buffer, const rpcconn_hdr_t* header)
{
	BOOL status = FALSE;
	size_t length = 0;
	RtsPduSignature signature = { 0 };
	RpcVirtualConnection* connection = NULL;

	WINPR_ASSERT(rpc);
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(header);

	wLog* log = WLog_Get(TAG);

	const size_t total = Stream_Length(buffer);
	length = header->common.frag_length;
	if (total < length)
	{
		WLog_Print(log, WLOG_ERROR, "PDU length %" PRIuz " does not match available data %" PRIuz,
		           length, total);
		return FALSE;
	}

	connection = rpc->VirtualConnection;

	if (!connection)
	{
		WLog_Print(log, WLOG_ERROR, "not connected, aborting");
		return FALSE;
	}

	if (!rts_extract_pdu_signature(&signature, buffer, header))
		return FALSE;

	rts_print_pdu_signature(log, WLOG_TRACE, &signature);

	if (memcmp(&signature, &RTS_PDU_FLOW_CONTROL_ACK_SIGNATURE, sizeof(signature)) == 0)
	{
		status = rts_recv_flow_control_ack_pdu(rpc, buffer);
	}
	else if (memcmp(&signature, &RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION_SIGNATURE,
	                sizeof(signature)) == 0)
	{
		status = rts_recv_flow_control_ack_with_destination_pdu(rpc, buffer);
	}
	else if (memcmp(&signature, &RTS_PDU_PING_SIGNATURE, sizeof(signature)) == 0)
	{
		status = rts_send_ping_pdu(rpc);
	}
	else
	{
		if (connection->DefaultOutChannel->State == CLIENT_OUT_CHANNEL_STATE_OPENED)
		{
			if (memcmp(&signature, &RTS_PDU_OUT_R1_A2_SIGNATURE, sizeof(signature)) == 0)
			{
				status = rts_recv_OUT_R1_A2_pdu(rpc, buffer);
			}
		}
		else if (connection->DefaultOutChannel->State == CLIENT_OUT_CHANNEL_STATE_OPENED_A6W)
		{
			if (memcmp(&signature, &RTS_PDU_OUT_R2_A6_SIGNATURE, sizeof(signature)) == 0)
			{
				status = rts_recv_OUT_R2_A6_pdu(rpc, buffer);
			}
		}
		else if (connection->DefaultOutChannel->State == CLIENT_OUT_CHANNEL_STATE_OPENED_B3W)
		{
			if (memcmp(&signature, &RTS_PDU_OUT_R2_B3_SIGNATURE, sizeof(signature)) == 0)
			{
				status = rts_recv_OUT_R2_B3_pdu(rpc, buffer);
			}
		}
	}

	if (!status)
	{
		const UINT32 SignatureId = rts_identify_pdu_signature(&signature, NULL);
		WLog_Print(log, WLOG_ERROR, "error parsing RTS PDU with signature id: 0x%08" PRIX32 "",
		           SignatureId);
		rts_print_pdu_signature(log, WLOG_ERROR, &signature);
	}

	const size_t rem = Stream_GetRemainingLength(buffer);
	if (rem > 0)
	{
		WLog_Print(log, WLOG_ERROR, "%" PRIuz " bytes or %" PRIuz " total not parsed, aborting",
		           rem, total);
		rts_print_pdu_signature(log, WLOG_ERROR, &signature);
		return FALSE;
	}

	return status;
}

BOOL rts_write_pdu_auth3(wStream* s, const rpcconn_rpc_auth_3_hdr_t* auth)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(auth);

	if (!rts_write_common_pdu_header(s, &auth->header))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 2 * sizeof(UINT16)))
		return FALSE;

	Stream_Write_UINT16(s, auth->max_xmit_frag);
	Stream_Write_UINT16(s, auth->max_recv_frag);

	return rts_write_auth_verifier(s, &auth->auth_verifier, &auth->header);
}

BOOL rts_write_pdu_bind(wStream* s, const rpcconn_bind_hdr_t* bind)
{

	WINPR_ASSERT(s);
	WINPR_ASSERT(bind);

	if (!rts_write_common_pdu_header(s, &bind->header))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, bind->max_xmit_frag);
	Stream_Write_UINT16(s, bind->max_recv_frag);
	Stream_Write_UINT32(s, bind->assoc_group_id);

	if (!rts_write_context_list(s, &bind->p_context_elem))
		return FALSE;

	return rts_write_auth_verifier(s, &bind->auth_verifier, &bind->header);
}

BOOL rts_conditional_check_and_log(const char* tag, wStream* s, size_t size, BOOL silent,
                                   const char* fkt, const char* file, size_t line)
{
	if (silent)
	{
		const size_t rem = Stream_GetRemainingLength(s);
		if (rem < size)
			return FALSE;
		return TRUE;
	}

	return Stream_CheckAndLogRequiredLengthEx(tag, WLOG_WARN, s, size, 1, "%s(%s:%" PRIuz ")", fkt,
	                                          file, line);
}

BOOL rts_conditional_safe_seek(wStream* s, size_t size, BOOL silent, const char* fkt,
                               const char* file, size_t line)
{
	if (silent)
	{
		const size_t rem = Stream_GetRemainingLength(s);
		if (rem < size)
			return FALSE;
	}
	return Stream_SafeSeekEx(s, size, file, line, fkt);
}
