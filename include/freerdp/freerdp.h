/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Interface
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_H
#define FREERDP_H

#include <winpr/stream.h>
#include <winpr/sspi.h>

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/error.h>
#include <freerdp/event.h>

#include <freerdp/settings.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/codecs.h>
#include <freerdp/metrics.h>
#include <freerdp/extension.h>
#include <freerdp/heartbeat.h>
#include <freerdp/message.h>
#include <freerdp/autodetect.h>
#include <freerdp/streamdump.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rdp_rdp rdpRdp;
	typedef struct rdp_rail rdpRail;
	typedef struct rdp_cache rdpCache;

	typedef struct rdp_client_context rdpClientContext;
	typedef struct rdp_client_entry_points_v1 RDP_CLIENT_ENTRY_POINTS_V1;
	typedef RDP_CLIENT_ENTRY_POINTS_V1 RDP_CLIENT_ENTRY_POINTS;

#include <freerdp/utils/smartcardlogon.h>
#include <freerdp/update.h>
#include <freerdp/input.h>
#include <freerdp/graphics.h>

#define MCS_BASE_CHANNEL_ID 1001
#define MCS_GLOBAL_CHANNEL_ID 1003

/* Flags used by certificate callbacks */
#define VERIFY_CERT_FLAG_NONE 0x00
#define VERIFY_CERT_FLAG_LEGACY 0x02
#define VERIFY_CERT_FLAG_REDIRECT 0x10
#define VERIFY_CERT_FLAG_GATEWAY 0x20
#define VERIFY_CERT_FLAG_CHANGED 0x40
#define VERIFY_CERT_FLAG_MISMATCH 0x80
#define VERIFY_CERT_FLAG_MATCH_LEGACY_SHA1 0x100
#define VERIFY_CERT_FLAG_FP_IS_PEM 0x200

/* Message types used by gateway messaging callback */
#define GATEWAY_MESSAGE_CONSENT 1
#define GATEWAY_MESSAGE_SERVICE 2

	typedef enum
	{
		AUTH_NLA,
		AUTH_TLS,
		AUTH_RDP,
		GW_AUTH_HTTP,
		GW_AUTH_RDG,
		GW_AUTH_RPC,
		AUTH_SMARTCARD_PIN
	} rdp_auth_reason;

	typedef BOOL (*pContextNew)(freerdp* instance, rdpContext* context);
	typedef void (*pContextFree)(freerdp* instance, rdpContext* context);

	typedef BOOL (*pConnectCallback)(freerdp* instance);
	typedef void (*pPostDisconnect)(freerdp* instance);

	/** \brief Authentication callback function pointer definition
	 *
	 * \param instance A pointer to the instance to work on
	 * \param username A pointer to the username string. On input the current username, on output
	 * the username that should be used. Must not be NULL. \param password A pointer to the password
	 * string. On input the current password, on output the password that sohould be used. Must not
	 * be NULL. \param domain A pointer to the domain string. On input the current domain, on output
	 * the domain that sohould be used. Must not be NULL.
	 *
	 * \return \b FALSE no valid credentials supplied, continue without \b TRUE valid credentials
	 * should be available.
	 */

	typedef BOOL (*pAuthenticate)(freerdp* instance, char** username, char** password,
	                              char** domain);

	/** \brief Extended authentication callback function pointer definition
	 *
	 * \param instance A pointer to the instance to work on
	 * \param username A pointer to the username string. On input the current username, on output
	 * the username that should be used. Must not be NULL. \param password A pointer to the password
	 * string. On input the current password, on output the password that sohould be used. Must not
	 * be NULL. \param domain A pointer to the domain string. On input the current domain, on output
	 * the domain that sohould be used. Must not be NULL. \param reason The reason the callback was
	 * called. (e.g. NLA, TLS, RDP, GATEWAY, ...)
	 *
	 * \return \b FALSE to abort the connection, \b TRUE otherwise.
	 * \note To not provide valid credentials and not abort the connection return \b TRUE and empty
	 * (as in empty string) credentials
	 */
	typedef BOOL (*pAuthenticateEx)(freerdp* instance, char** username, char** password,
	                                char** domain, rdp_auth_reason reason);
	typedef BOOL (*pChooseSmartcard)(freerdp* instance, SmartcardCertInfo** cert_list, DWORD count,
	                                 DWORD* choice, BOOL gateway);

	typedef enum
	{
		ACCESS_TOKEN_TYPE_AAD, /**!< oauth2 access token for RDS AAD authentication */
		ACCESS_TOKEN_TYPE_AVD  /**!< oauth2 access token for Azure Virtual Desktop */
	} AccessTokenType;

	typedef BOOL (*pGetAccessToken)(freerdp* instance, AccessTokenType tokenType, char** token,
	                                size_t count, ...);

	/** @brief Callback used to inform about a reconnection attempt
	 *
	 *  @param instance The instance the information is for
	 *  @param what A '\0' terminated string describing the module attempting to retry an operation
	 *  @param current The current reconnection attempt, the first attempt will always have the
	 * value \b 0
	 *  @param userarg An optional custom argument
	 *
	 *  @return \b -1 in case of failure (attempts exceeded, ...) or a \b delay in [ms] to wait
	 * before the next attempt
	 */
	typedef SSIZE_T (*pRetryDialog)(freerdp* instance, const char* what, size_t current,
	                                void* userarg);

	/** @brief Callback used if user interaction is required to accept
	 *         an unknown certificate.
	 *
	 *  @deprecated Use pVerifyCertificateEx
	 *  @param common_name      The certificate registered hostname.
	 *  @param subject          The common name of the certificate.
	 *  @param issuer           The issuer of the certificate.
	 *  @param fingerprint      The fingerprint of the certificate (old) or the certificate in PEM
	 * format
	 *  @param host_mismatch    A flag indicating the certificate
	 *                          subject does not match the host connecting to.
	 *
	 *  @return 1 to accept and store a certificate, 2 to accept
	 *          a certificate only for this session, 0 otherwise.
	 */
#if defined(WITH_FREERDP_DEPRECATED)
	typedef WINPR_DEPRECATED_VAR(
	    "Use pVerifyCertificateEx",
	    DWORD (*pVerifyCertificate)(freerdp* instance, const char* common_name, const char* subject,
	                                const char* issuer, const char* fingerprint,
	                                BOOL host_mismatch));
#endif

	/** @brief Callback used if user interaction is required to accept
	 *         an unknown certificate.
	 *
	 *  @param host             The hostname connecting to.
	 *  @param port             The port connecting to.
	 *  @param common_name      The certificate registered hostname.
	 *  @param subject          The common name of the certificate.
	 *  @param issuer           The issuer of the certificate.
	 *  @param fingerprint      The fingerprint of the certificate (old) or the certificate in PEM
	 * format (VERIFY_CERT_FLAG_FP_IS_PEM set)
	 *  @param flags            Flags of type VERIFY_CERT_FLAG*
	 *
	 *  @return 1 to accept and store a certificate, 2 to accept
	 *          a certificate only for this session, 0 otherwise.
	 */
	typedef DWORD (*pVerifyCertificateEx)(freerdp* instance, const char* host, UINT16 port,
	                                      const char* common_name, const char* subject,
	                                      const char* issuer, const char* fingerprint, DWORD flags);

	/** @brief Callback used if user interaction is required to accept
	 *         a changed certificate.
	 *
	 *  @deprecated Use pVerifyChangedCertificateEx
	 *  @param common_name      The certificate registered hostname.
	 *  @param subject          The common name of the new certificate.
	 *  @param issuer           The issuer of the new certificate.
	 *  @param new_fingerprint  The fingerprint of the new certificate.
	 *  @param old_subject      The common name of the old certificate.
	 *  @param old_issuer       The issuer of the new certificate.
	 *  @param old_fingerprint  The fingerprint of the old certificate.
	 *
	 *  @return 1 to accept and store a certificate, 2 to accept
	 *          a certificate only for this session, 0 otherwise.
	 */
#if defined(WITH_FREERDP_DEPRECATED)
	typedef WINPR_DEPRECATED_VAR(
	    "Use pVerifyChangedCertificateEx",
	    DWORD (*pVerifyChangedCertificate)(freerdp* instance, const char* common_name,
	                                       const char* subject, const char* issuer,
	                                       const char* new_fingerprint, const char* old_subject,
	                                       const char* old_issuer, const char* old_fingerprint));
#endif

	/** @brief Callback used if user interaction is required to accept
	 *         a changed certificate.
	 *
	 *  @param host             The hostname connecting to.
	 *  @param port             The port connecting to.
	 *  @param common_name      The certificate registered hostname.
	 *  @param subject          The common name of the new certificate.
	 *  @param issuer           The issuer of the new certificate.
	 *  @param new_fingerprint  The fingerprint of the new certificate (old) or the certificate in
	 * PEM format (VERIFY_CERT_FLAG_FP_IS_PEM set)
	 *  @param old_subject      The common name of the old certificate.
	 *  @param old_issuer       The issuer of the new certificate.
	 *  @param old_fingerprint  The fingerprint of the old certificate (old) or the certificate in
	 * PEM format (VERIFY_CERT_FLAG_FP_IS_PEM set)
	 *  @param flags            Flags of type VERIFY_CERT_FLAG*
	 *
	 *  @return 1 to accept and store a certificate, 2 to accept
	 *          a certificate only for this session, 0 otherwise.
	 */

	typedef DWORD (*pVerifyChangedCertificateEx)(freerdp* instance, const char* host, UINT16 port,
	                                             const char* common_name, const char* subject,
	                                             const char* issuer, const char* new_fingerprint,
	                                             const char* old_subject, const char* old_issuer,
	                                             const char* old_fingerprint, DWORD flags);

	/** @brief Callback used if user interaction is required to accept
	 *         a certificate.
	 *
	 *  @param instance         Pointer to the freerdp instance.
	 *  @param data             Pointer to certificate data (full chain) in PEM format.
	 *  @param length           The length of the certificate data.
	 *  @param hostname         The hostname connecting to.
	 *  @param port             The port connecting to.
	 *  @param flags            Flags of type VERIFY_CERT_FLAG*
	 *
	 *  @return 1 to accept and store a certificate, 2 to accept
	 *          a certificate only for this session, 0 otherwise.
	 */
	typedef int (*pVerifyX509Certificate)(freerdp* instance, const BYTE* data, size_t length,
	                                      const char* hostname, UINT16 port, DWORD flags);

	typedef int (*pLogonErrorInfo)(freerdp* instance, UINT32 data, UINT32 type);

	typedef BOOL (*pSendChannelData)(freerdp* instance, UINT16 channelId, const BYTE* data,
	                                 size_t size);
	typedef BOOL (*pSendChannelPacket)(freerdp* instance, UINT16 channelId, size_t totalSize,
	                                   UINT32 flags, const BYTE* data, size_t chunkSize);
	typedef BOOL (*pReceiveChannelData)(freerdp* instance, UINT16 channelId, const BYTE* data,
	                                    size_t size, UINT32 flags, size_t totalSize);

	/* type can be one of the GATEWAY_MESSAGE_ type defines */
	typedef BOOL (*pPresentGatewayMessage)(freerdp* instance, UINT32 type, BOOL isDisplayMandatory,
	                                       BOOL isConsentMandatory, size_t length,
	                                       const WCHAR* message);

	/**
	 * Defines the context for a given instance of RDP connection.
	 * It is embedded in the rdp_freerdp structure, and allocated by a call to
	 * freerdp_context_new(). It is deallocated by a call to freerdp_context_free().
	 */
	struct rdp_context
	{
		ALIGN64 freerdp* instance;  /**< (offset 0)
		                       Pointer to a rdp_freerdp structure.
		                       This is a back-link to retrieve the freerdp instance from the context.
		                       It is set by the freerdp_context_new() function */
		ALIGN64 freerdp_peer* peer; /**< (offset 1)
		                       Pointer to the client peer.
		                       This is set by a call to freerdp_peer_context_new() during peer
		                       initialization. This field is used only on the server side. */
		ALIGN64 BOOL ServerMode;    /**< (offset 2) true when context is in server mode */

		ALIGN64 UINT32 LastError; /* 3 */

		UINT64 paddingA[16 - 4]; /* 4 */

		ALIGN64 int argc;    /**< (offset 16)
		                Number of arguments given to the program at launch time.
		                Used to keep this data available and used later on, typically just before
		                connection initialization.
		                @see freerdp_parse_args() */
		ALIGN64 char** argv; /**< (offset 17)
		                List of arguments given to the program at launch time.
		                Used to keep this data available and used later on, typically just before
		                connection initialization.
		                @see freerdp_parse_args() */

		ALIGN64 wPubSub* pubSub; /* (offset 18) */

		ALIGN64 HANDLE channelErrorEvent; /* (offset 19)*/
		ALIGN64 UINT channelErrorNum;     /*(offset 20)*/
		ALIGN64 char* errorDescription;   /*(offset 21)*/

		UINT64 paddingB[32 - 22]; /* 22 */

		ALIGN64 rdpRdp*
		    rdp;                           /**< (offset 32)
		                              Pointer to a rdp_rdp structure used to keep the connection's parameters.
		                              It is allocated by freerdp_context_new() and deallocated by
		                              freerdp_context_free(), at the same		       time that this rdp_context
		                              structure -		       there is no need to specifically allocate/deallocate this. */
		ALIGN64 rdpGdi* gdi;               /**< (offset 33)
		                              Pointer to a rdp_gdi structure used to keep the gdi settings.
		                              It is allocated by gdi_init() and deallocated by gdi_free().
		                              It must be deallocated before deallocating this rdp_context structure. */
		ALIGN64 rdpRail* rail;             /* 34 */
		ALIGN64 rdpCache* cache;           /* 35 */
		ALIGN64 rdpChannels* channels;     /* 36 */
		ALIGN64 rdpGraphics* graphics;     /* 37 */
		ALIGN64 rdpInput* input;           /* 38 owned by rdpRdp */
		ALIGN64 rdpUpdate* update;         /* 39 owned by rdpRdp */
		ALIGN64 rdpSettings* settings;     /* 40 owned by rdpRdp */
		ALIGN64 rdpMetrics* metrics;       /* 41 */
		ALIGN64 rdpCodecs* codecs;         /* 42 */
		ALIGN64 rdpAutoDetect* autodetect; /* 43 owned by rdpRdp */
		UINT64 paddingC1[45 - 44];         /* 44 */
		ALIGN64 int disconnectUltimatum;   /* 45 */
		UINT64 paddingC[64 - 46];          /* 46 */

		ALIGN64 rdpStreamDumpContext* dump; /* 64 */
		ALIGN64 wLog* log;                  /* 65 */

		UINT64 paddingD[96 - 66];  /* 66 */
		UINT64 paddingE[128 - 96]; /* 96 */
	};

	/**
	 *  Defines the possible disconnect reasons in the MCS Disconnect Provider
	 *  Ultimatum PDU
	 *
	 *  [T.125] 7 Structure of Version 2 MCSPDUs Reason ::= ENUMERATED
	 */

	enum Disconnect_Ultimatum
	{
		Disconnect_Ultimatum_domain_disconnected = 0,
		Disconnect_Ultimatum_provider_initiated = 1,
		Disconnect_Ultimatum_token_purged = 2,
		Disconnect_Ultimatum_user_requested = 3,
		Disconnect_Ultimatum_channel_purged = 4
	};

#include <freerdp/client.h>

	/** Defines the options for a given instance of RDP connection.
	 *  This is built by the client and given to the FreeRDP library to create the connection
	 *  with the expected options.
	 *  It is allocated by a call to freerdp_new() and deallocated by a call to freerdp_free().
	 *  Some of its content need specific allocation/deallocation - see field description for
	 * details.
	 */
	struct rdp_freerdp
	{
		ALIGN64
		rdpContext* context; /**< (offset 0)
		                  Pointer to a rdpContext structure.
		                  Client applications can use the ContextSize field to register a
		                  context bigger than the rdpContext structure. This allow clients to
		                  use additional context information. When using this capability, client
		                  application should ALWAYS declare their structure with the rdpContext
		                  field first, and any additional content following it. Can be allocated
		                  by a call to freerdp_context_new(). Must be deallocated by a call to
		                  freerdp_context_free() before deallocating the current instance. */

		ALIGN64 RDP_CLIENT_ENTRY_POINTS* pClientEntryPoints;

		UINT64 paddingA[16 - 2]; /* 2 */

#if defined(WITH_FREERDP_DEPRECATED)
		WINPR_DEPRECATED_VAR("use rdpContext::input instead", ALIGN64 rdpInput* input;) /* (offset
		                        16) Input handle for the connection. Will be initialized by a call
		                        to freerdp_context_new() owned by rdpRdp */
		WINPR_DEPRECATED_VAR("use rdpContext::update instead",
		                     ALIGN64 rdpUpdate* update;) /* (offset 17)
		                      Update display parameters. Used to register display events callbacks
and settings.		 Will be initialized by a call to freerdp_context_new()		 owned by rdpRdp */
		WINPR_DEPRECATED_VAR("use rdpContext::settings instead",
		                     ALIGN64 rdpSettings* settings;) /**< (offset 18)
		                            Pointer to a rdpSettings structure. Will be used to maintain the
		                            required RDP	 settings.		              Will be
		                            initialized by	 a call to freerdp_context_new()
		                            owned by rdpRdp
		                          */
		WINPR_DEPRECATED_VAR("use rdpContext::autodetect instead",
		                     ALIGN64 rdpAutoDetect* autodetect;) /* (offset 19)
		                                Auto-Detect handle for the connection.
		                                Will be initialized by a call to freerdp_context_new()
owned by rdpRdp */
#else
	UINT64 paddingX[4];
#endif
		ALIGN64 rdpHeartbeat* heartbeat; /* (offset 21) owned by rdpRdp*/

		UINT64 paddingB[32 - 21]; /* 21 */

		ALIGN64 size_t
		    ContextSize; /* (offset 32)
		             Specifies the size of the 'context' field. freerdp_context_new() will use this
		             size to allocate the context buffer. freerdp_new() sets it to
		             sizeof(rdpContext). If modifying it, there should always be a minimum of
		             sizeof(rdpContext), as the freerdp library will assume it can use the 'context'
		             field to set the required information in it. Clients will typically make it
		             bigger, and use a context structure embedding the rdpContext, and adding
		             additional information after that.
		          */

		ALIGN64 pContextNew
		    ContextNew; /**< (offset 33)
		             Callback for context allocation
		             Can be set before calling freerdp_context_new() to have it executed after
		             allocation and initialization. Must be set to NULL if not needed. */

		ALIGN64 pContextFree
		    ContextFree;          /**< (offset 34)
		                       Callback for context deallocation
		                       Can be set before calling freerdp_context_free() to have it executed before
		                       deallocation.		  Must be set to NULL if not needed. */
		UINT64 paddingC[47 - 35]; /* 35 */

		ALIGN64 UINT ConnectionCallbackState; /* 47 */

		ALIGN64 pConnectCallback
		    PreConnect; /**< (offset 48)
		             Callback for pre-connect operations.
		             Can be set before calling freerdp_connect() to have it executed before the
		             actual connection happens. Must be set to NULL if not needed. */

		ALIGN64 pConnectCallback
		    PostConnect; /**< (offset 49)
		              Callback for post-connect operations.
		              Can be set before calling freerdp_connect() to have it executed after the
		              actual connection has succeeded. Must be set to NULL if not needed. */

		ALIGN64 pAuthenticate Authenticate; /**< (offset 50)
		                                 Callback for authentication.
		                                 It is used to get the username/password when it was not
		                                 provided at connection time. */
#if defined(WITH_FREERDP_DEPRECATED)
		WINPR_DEPRECATED_VAR("Use VerifyCertificateEx or VerifyX509Certificate  instead",
		                     ALIGN64 pVerifyCertificate VerifyCertificate;) /**< (offset 51) */
		WINPR_DEPRECATED_VAR("Use VerifyChangedCertificateEx or VerifyX509Certificate  instead",
		                     ALIGN64 pVerifyChangedCertificate
		                         VerifyChangedCertificate;) /**< (offset 52) */
#else
	ALIGN64 UINT64 reserved[2];
#endif
		ALIGN64 pVerifyX509Certificate
		    VerifyX509Certificate; /**< (offset 53)  Callback for X509 certificate verification
		                              (PEM format) */

		ALIGN64 pLogonErrorInfo
		    LogonErrorInfo; /**< (offset 54)  Callback for logon error info, important for logon
		                       system messages with RemoteApp */

		ALIGN64 pPostDisconnect
		    PostDisconnect; /**< (offset 55)
		                                                                Callback for cleaning up
		                       resources allocated by post connect callback.

		                       This will be called before disconnecting and cleaning up the
		                       channels.
 */

		ALIGN64 pAuthenticate GatewayAuthenticate; /**< (offset 56)
		                                 Callback for gateway authentication.
		                                 It is used to get the username/password when it was not
		                                 provided at connection time. */

		ALIGN64 pPresentGatewayMessage PresentGatewayMessage; /**< (offset 57)
		                                  Callback for gateway consent messages.
		                                  It is used to present consent messages to the user. */

		ALIGN64 pConnectCallback Redirect; /**< (offset 58)
		                                               Callback for redirect operations.
		                                               Can be set after
		             rdp_client_disconnect_and_clear and applying redirection settings but before
		             rdp_client_connect() to have it executed after the actual connection has
		             succeeded. Must be set to NULL if not needed. */
		ALIGN64 pConnectCallback
		    LoadChannels; /**< (offset 59)
		                   * callback for loading channel configuration. Might be called multiple
		                   * times when redirection occurs. */

		ALIGN64 pPostDisconnect
		    PostFinalDisconnect;  /** < (offset 60)
		                           * callback for cleaning up resources allocated in PreConnect
		                           *
		                           * This will be called after all instance related channels and
		                           * threads have been stopped
		                           */
		UINT64 paddingD[64 - 61]; /* 61 */

		ALIGN64 pSendChannelData
		    SendChannelData; /* (offset 64)
		                Callback for sending data to a channel.
		                By default, it is set by freerdp_new() to freerdp_send_channel_data(), which
		                eventually calls freerdp_channel_send() */
		ALIGN64 pReceiveChannelData
		    ReceiveChannelData; /* (offset 65)
		                   Callback for receiving data from a channel.
		                   This is called by freerdp_channel_process() (if not NULL).
		                   Clients will typically use a function that calls freerdp_channels_data()
		                   to perform the needed tasks. */

		ALIGN64 pVerifyCertificateEx
		    VerifyCertificateEx; /**< (offset 66)
		                  Callback for certificate validation.
		                  Used to verify that an unknown certificate is trusted. */
		ALIGN64 pVerifyChangedCertificateEx
		    VerifyChangedCertificateEx; /**< (offset 67)
		                         Callback for changed certificate validation.
		                         Used when a certificate differs from stored fingerprint. */
		ALIGN64 pSendChannelPacket
		    SendChannelPacket;                  /* (offset 68)
		                                         * Callback for sending RAW data to a channel. In contrast to
		                                         * SendChannelData data fragmentation    is up to the user and this
		                                         * function sends data as is with the provided flags.
		                                         */
		ALIGN64 pAuthenticateEx AuthenticateEx; /**< (offset 69)
		                                 Callback for authentication.
		                                 It is used to get the username/password. The reason
		                                 argument tells why it was called.  */
		ALIGN64 pChooseSmartcard
		    ChooseSmartcard;                    /* (offset 70)
		                                      Callback for choosing a smartcard for logon.
		                                      Used when multiple smartcards are available. Returns an index into a list
		                                      of SmartcardCertInfo pointers	*/
		ALIGN64 pGetAccessToken GetAccessToken; /* (offset 71)
		                                            Callback for obtaining an access token
		                                            for \b AccessTokenType authentication */
		ALIGN64 pRetryDialog RetryDialog; /* (offset 72) Callback for displaying a dialog in case of
		                                     something needs a retry */
		UINT64 paddingE[80 - 73];         /* 73 */
	};

	struct rdp_channel_handles
	{
		wListDictionary* init;
		wListDictionary* open;
	};
	typedef struct rdp_channel_handles rdpChannelHandles;

	FREERDP_API void freerdp_context_free(freerdp* instance);

	FREERDP_API BOOL freerdp_context_new(freerdp* instance);
	FREERDP_API BOOL freerdp_context_new_ex(freerdp* instance, rdpSettings* settings);

	FREERDP_API BOOL freerdp_context_reset(freerdp* instance);

	FREERDP_API BOOL freerdp_connect(freerdp* instance);

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
	WINPR_DEPRECATED_VAR("use freerdp_abort_connect_context instead",
	                     FREERDP_API BOOL freerdp_abort_connect(freerdp* instance));
#endif

	FREERDP_API BOOL freerdp_abort_connect_context(rdpContext* context);
	FREERDP_API HANDLE freerdp_abort_event(rdpContext* context);

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
	WINPR_DEPRECATED_VAR("use freerdp_shall_disconnect_context instead",
	                     FREERDP_API BOOL freerdp_shall_disconnect(freerdp* instance));
#endif

	FREERDP_API BOOL freerdp_shall_disconnect_context(const rdpContext* context);
	FREERDP_API BOOL freerdp_disconnect(freerdp* instance);

	/** @brief stringify disconnect reason of type Disconnect_Ultimatum
	 *
	 *  @param reason the reason of type \b Disconnect_Ultimatum
	 *
	 *  @return a string representation of \b reason or rn-unknown
	 *
	 *  @since version 3.13.0
	 */
	FREERDP_API const char* freerdp_disconnect_reason_string(int reason);

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
	WINPR_DEPRECATED_VAR("use freerdp_disconnect_before_reconnect_context instead",
	                     FREERDP_API BOOL freerdp_disconnect_before_reconnect(freerdp* instance));
#endif

	FREERDP_API BOOL freerdp_disconnect_before_reconnect_context(rdpContext* context);

	FREERDP_API BOOL freerdp_reconnect(freerdp* instance);

	FREERDP_API UINT freerdp_channels_attach(freerdp* instance);
	FREERDP_API UINT freerdp_channels_detach(freerdp* instance);

#if defined(WITH_FREERDP_DEPRECATED)
	WINPR_DEPRECATED_VAR("Use freerdp_get_event_handles",
	                     FREERDP_API BOOL freerdp_get_fds(freerdp* instance, void** rfds,
	                                                      int* rcount, void** wfds, int* wcount));
#endif

	FREERDP_API BOOL freerdp_check_fds(freerdp* instance);

	FREERDP_API DWORD freerdp_get_event_handles(rdpContext* context, HANDLE* events, DWORD count);
	FREERDP_API BOOL freerdp_check_event_handles(rdpContext* context);

	FREERDP_API wMessageQueue* freerdp_get_message_queue(freerdp* instance, DWORD id);
	FREERDP_API HANDLE freerdp_get_message_queue_event_handle(freerdp* instance, DWORD id);
	FREERDP_API int freerdp_message_queue_process_message(freerdp* instance, DWORD id,
	                                                      wMessage* message);
	FREERDP_API int freerdp_message_queue_process_pending_messages(freerdp* instance, DWORD id);

	FREERDP_API UINT32 freerdp_error_info(freerdp* instance);
	FREERDP_API void freerdp_set_error_info(rdpRdp* rdp, UINT32 error);
	FREERDP_API BOOL freerdp_send_error_info(rdpRdp* rdp);
	FREERDP_API BOOL freerdp_get_stats(rdpRdp* rdp, UINT64* inBytes, UINT64* outBytes,
	                                   UINT64* inPackets, UINT64* outPackets);

	FREERDP_API void freerdp_get_version(int* major, int* minor, int* revision);
	FREERDP_API const char* freerdp_get_version_string(void);
	FREERDP_API const char* freerdp_get_build_revision(void);
	FREERDP_API const char* freerdp_get_build_config(void);

	FREERDP_API void freerdp_free(freerdp* instance);

	WINPR_ATTR_MALLOC(freerdp_free, 1)
	FREERDP_API freerdp* freerdp_new(void);

	FREERDP_API BOOL freerdp_focus_required(freerdp* instance);
	FREERDP_API void freerdp_set_focus(freerdp* instance);

	FREERDP_API int freerdp_get_disconnect_ultimatum(rdpContext* context);

	FREERDP_API UINT32 freerdp_get_last_error(rdpContext* context);
	FREERDP_API const char* freerdp_get_last_error_name(UINT32 error);
	FREERDP_API const char* freerdp_get_last_error_string(UINT32 error);
	FREERDP_API const char* freerdp_get_last_error_category(UINT32 error);

#define freerdp_set_last_error(context, lastError) \
	freerdp_set_last_error_ex((context), (lastError), __func__, __FILE__, __LINE__)

#define freerdp_set_last_error_if_not(context, lastError)             \
	do                                                                \
	{                                                                 \
		if (freerdp_get_last_error(context) == FREERDP_ERROR_SUCCESS) \
			freerdp_set_last_error_log(context, lastError);           \
	} while (0)

#define freerdp_set_last_error_log(context, lastError) \
	freerdp_set_last_error_ex((context), (lastError), __func__, __FILE__, __LINE__)
	FREERDP_API void freerdp_set_last_error_ex(rdpContext* context, UINT32 lastError,
	                                           const char* fkt, const char* file, int line);

	FREERDP_API const char* freerdp_get_logon_error_info_type(UINT32 type);
	FREERDP_API const char* freerdp_get_logon_error_info_type_ex(UINT32 type, char* buffer,
	                                                             size_t size);

	FREERDP_API const char* freerdp_get_logon_error_info_data(UINT32 data);
	FREERDP_API const char* freerdp_get_logon_error_info_data_ex(UINT32 data, char* buffer,
	                                                             size_t size);

	FREERDP_API ULONG freerdp_get_transport_sent(rdpContext* context, BOOL resetCount);

	FREERDP_API BOOL freerdp_nla_impersonate(rdpContext* context);
	FREERDP_API BOOL freerdp_nla_revert_to_self(rdpContext* context);

	FREERDP_API UINT32 freerdp_get_nla_sspi_error(rdpContext* context);

	/** Encrypts the provided buffer using the NLA's GSSAPI context
	 *
	 *	\param context the RDP context
	 *	\param inBuffer the SecBuffer buffer to encrypt
	 *	\param outBuffer a SecBuffer to hold the encrypted content
	 *	\returns if the operation completed successfully
	 *	\since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_nla_encrypt(rdpContext* context, const SecBuffer* inBuffer,
	                                     SecBuffer* outBuffer);

	/** Decrypts the provided buffer using the NLA's GSSAPI context
	 *
	 *	\param context the RDP context
	 *	\param inBuffer the SecBuffer buffer to decrypt
	 *	\param outBuffer a SecBuffer to hold the decrypted content
	 *	\returns if the operation completed successfully
	 *	\since version 3.9.0
	 */
	FREERDP_API BOOL freerdp_nla_decrypt(rdpContext* context, const SecBuffer* inBuffer,
	                                     SecBuffer* outBuffer);

	/** Calls QueryContextAttributes on the SSPI context associated with the NLA part of
	 * the RDP context
	 *
	 *	\param context the RDP context
	 *	\param ulAttr the attribute
	 *	\param pBuffer an opaque pointer depending on ulAttr
	 *	\returns a SECURITY_STATUS indicating if the operation completed successfully
	 *	\since version 3.9.0
	 */
	FREERDP_API SECURITY_STATUS freerdp_nla_QueryContextAttributes(rdpContext* context,
	                                                               DWORD ulAttr, PVOID pBuffer);

	FREERDP_API void clearChannelError(rdpContext* context);
	FREERDP_API HANDLE getChannelErrorEventHandle(rdpContext* context);
	FREERDP_API UINT getChannelError(rdpContext* context);
	FREERDP_API const char* getChannelErrorDescription(rdpContext* context);
	FREERDP_API void setChannelError(rdpContext* context, UINT errorNum, const char* format, ...);
	FREERDP_API BOOL checkChannelErrorEvent(rdpContext* context);

	FREERDP_API const char* freerdp_nego_get_routing_token(rdpContext* context, DWORD* length);

	/** \brief returns the current \b CONNECTION_STATE of the context.
	 *
	 *  \param context A pointer to the context to query state
	 *
	 *  \return A \b CONNECTION_STATE the context is currently in
	 */
	FREERDP_API CONNECTION_STATE freerdp_get_state(const rdpContext* context);

	/** \brief returns a string representation of a \b CONNECTION_STATE
	 *
	 *  \param state the \b CONNECTION_STATE to stringify
	 *
	 *  \return The string representation of the \b CONNECTION_STATE
	 */
	FREERDP_API const char* freerdp_state_string(CONNECTION_STATE state);

	/** \brief Queries if the current \b CONNECTION_STATE of the context is an active connection.
	 *
	 * A connection is active, if the connection sequence has been passed, no disconnection requests
	 * have been received and no network or other errors have forced a disconnect.
	 *
	 *  \param context A pointer to the context to query state
	 *
	 *  \return \b TRUE if the connection state indicates an active connection, \b FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_is_active_state(const rdpContext* context);

	FREERDP_API BOOL freerdp_channels_from_mcs(rdpSettings* settings, const rdpContext* context);

	FREERDP_API BOOL freerdp_is_valid_mcs_create_request(const BYTE* data, size_t size);
	FREERDP_API BOOL freerdp_is_valid_mcs_create_response(const BYTE* data, size_t size);

	/** \brief Persist the current credentials (gateway, target server, ...)
	 *
	 *  FreeRDP internally keeps a backup of connection settings to revert to whenever a reconnect
	 * is required. If a client modifies settings during runtime after pre-connect call this
	 * function or the credentials will be lost on any reconnect, redirect, ...
	 *
	 *  \param context The RDP context to use, must not be \b NULL
	 *
	 *  \return \b TRUE if successful, \b FALSE if settings could not be applied (wrong session
	 * state, ...)
	 *  \since version 3.12.0
	 */
	FREERDP_API BOOL freerdp_persist_credentials(rdpContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_H */
