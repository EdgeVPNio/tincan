/*
 * EdgeVPNio
 * Copyright 2023, University of Florida
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef TINCAN_VIRTUAL_LINK_H_
#define TINCAN_VIRTUAL_LINK_H_
#include "tincan_base.h"
#include "buffer_pool.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/network.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/thread.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/dtls_transport.h"
#include "p2p/base/dtls_transport_factory.h"
#include "p2p/base/dtls_transport_internal.h"
#include "pc/jsep_transport_controller.h"
#include "p2p/base/packet_transport_internal.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/client/basic_port_allocator.h"
#include "peer_descriptor.h"
#include "turn_descriptor.h"

namespace tincan
{
    using namespace rtc;
    using cricket::ConnectionRole;
    using cricket::IceTransportInternal;
    using cricket::JsepTransportDescription;
    using cricket::P2PTransportChannel;
    using rtc::PacketTransportInternal;
    using webrtc::JsepTransportController;
    using webrtc::SdpType;

    struct VlinkDescriptor
    {
        bool dtls_enabled = true;
        string uid;
        vector<string> stun_servers;
        vector<TurnDescriptor> turn_descs;
    };

    class VirtualLink : public JsepTransportController::Observer,
                        public sigslot::has_slots<>
    {
    public:
        VirtualLink(
            unique_ptr<VlinkDescriptor> vlink_desc,
            unique_ptr<PeerDescriptor> peer_desc,
            rtc::Thread *signaling_thread,
            rtc::Thread *network_thread);

        string Name();

        void Initialize(
            unique_ptr<SSLIdentity> sslid,
            unique_ptr<SSLFingerprint> local_fingerprint,
            cricket::IceRole ice_role,
            const vector<string> &ignored_list);

        PeerDescriptor &PeerInfo()
        {
            return *peer_desc_.get();
        }

        void StartConnections();

        void Disconnect();

        bool IsReady();

        void Transmit(Iob&& frame);

        string Candidates();

        string PeerCandidates();

        void PeerCandidates(const string &peer_cas);

        string Id()
        {
            return vlink_desc_->uid;
        }
        void GetStats(Json::Value &infos);

        cricket::IceRole IceRole()
        {
            return ice_role_;
        }


        bool InitializePortAllocator();

        // JsepTransportController::Observer override.
        bool OnTransportChanged(
            const std::string &mid,
            webrtc::RtpTransportInternal *rtp_transport,
            rtc::scoped_refptr<webrtc::DtlsTransport> dtls_transport,
            webrtc::DataChannelTransportInterface *data_channel_transport) override;

        void SetCasReadyId(uint64_t id) noexcept;

        sigslot::signal1<string, single_threaded> SignalLinkUp;
        sigslot::signal1<string, single_threaded> SignalLinkDown;
        sigslot::signal2<uint64_t, string> SignalLocalCasReady;
        sigslot::signal2<const char *, size_t> SignalMessageReceived;

    private:
        cricket::ServerAddresses SetupSTUN(
            vector<string> stun_servers);

        vector<cricket::RelayServerConfig> SetupTURN(
            vector<TurnDescriptor>);

        void OnCandidatesGathered(
            const string &transport_name,
            const cricket::Candidates &candidates);

        void OnGatheringState(
            cricket::IceGatheringState gather_state);

        void OnWriteableState(
            PacketTransportInternal *transport);

        void RegisterLinkEventHandlers();

        void AddRemoteCandidates(
            const string &candidates);

        void SetupICE(
            unique_ptr<SSLIdentity> sslid,
            unique_ptr<SSLFingerprint> local_fingerprint,
            cricket::IceRole ice_role);

        void OnReadPacket(
            PacketTransportInternal *transport,
            const char *data,
            size_t len,
            const int64_t &ptime,
            int flags);

        void OnSentPacket(
            PacketTransportInternal *transport,
            const SentPacket &packet);

        static const char kCandidateDelim = ':';
        const string kIceUfrag = {"+001EVIOICEUFRAG"};
        const string kIcePwd = {"+00000001EVIOICEPASSWORD"};
        unique_ptr<VlinkDescriptor> vlink_desc_;
        unique_ptr<PeerDescriptor> peer_desc_;
        cricket::Candidates local_candidates_;
        cricket::IceRole ice_role_;
        ConnectionRole local_conn_role_;
        cricket::DtlsTransportInternal *dtls_transport_;
        unique_ptr<cricket::SessionDescription> local_description_;
        unique_ptr<cricket::SessionDescription> remote_description_;
        unique_ptr<SSLFingerprint> remote_fingerprint_;
        string content_name_;
        const PacketOptions packet_options_;
        JsepTransportController::Config config_;
        rtc::Thread *signaling_thread_;
        rtc::Thread *network_thread_;
        uint64_t cas_ready_id_;
        bool pa_init_;
        rtc::BasicNetworkManager net_manager_;
        unique_ptr<cricket::PortAllocator> port_allocator_;
        unique_ptr<webrtc::IceTransportFactory> ice_transport_factory_;
        unique_ptr<JsepTransportController> transport_ctlr_;
    };
} // namespace tincan
#endif // !TINCAN_VIRTUAL_LINK_H_
