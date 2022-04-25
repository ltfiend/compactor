/*
 * Copyright 2018-2021 Internet Corporation for Assigned Names and Numbers, Sinodun IT.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>

#include <ctemplate/template.h>
#include <ctemplate/template_modifiers.h>

#include "config.h"

#include "capturedns.hpp"
#include "geoip.hpp"
#include "log.hpp"

#include "template-backend.hpp"

#include <iostream>

/**
 ** Modifiers
 **/

namespace
{
    class CStringModifier : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            const char* pos = in;
            const char* const limit = in + inlen;

            while (pos < limit) {
                switch (*pos) {
                case '\0':
                    out->Emit("\\0");
                    break;

                case '\b':
                    out->Emit("\\b");
                    break;

                case '\t':
                    out->Emit("\\t");
                    break;

                case '\n':
                    out->Emit("\\n");
                    break;

                case '\r':
                    out->Emit("\\r");
                    break;

                case '\\':
                    out->Emit("\\\\");
                    break;

                case '"':
                    out->Emit("\\\"");
                    break;

                case '\'':
                    out->Emit("\\'");
                    break;

                default:
                    if ( std::use_facet<std::ctype<char>>(loc).is(std::ctype<char>::print, *pos) )
                    {
                        out->Emit(pos, 1);
                    }
                    else
                    {
                        out->Emit("\\x");

                        char buf[2];
                        unsigned char nyb = (*pos >> 4) & 0xf;
                        buf[0] = ( nyb > 9 ) ? 'a' + nyb - 10 : '0' + nyb;
                        nyb = *pos & 0xf;
                        buf[1] = ( nyb > 9 ) ? 'a' + nyb - 10 : '0' + nyb;
                        out->Emit(buf, 2);
                    }
                    break;
                }
                pos++;
            }
        }

    private:
        std::locale loc;
    };

    class HexStringModifier : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            const char* pos = in;
            const char* const limit = in + inlen;

            while (pos < limit) {
                switch (*pos) {
                case '\0':
                    out->Emit("\\0");
                    break;

                default:
                    out->Emit("\\x");

                    char buf[2];
                    unsigned char nyb = (*pos >> 4) & 0xf;
                    buf[0] = ( nyb > 9 ) ? 'a' + nyb - 10 : '0' + nyb;
                    nyb = *pos & 0xf;
                    buf[1] = ( nyb > 9 ) ? 'a' + nyb - 10 : '0' + nyb;
                    out->Emit(buf, 2);
                    break;
                }
                pos++;
            }
        }
    };

 /**
  ** Modifiers
  **/
 
    class ShowNonprintable : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* per_expand_data,
                            ctemplate::ExpandEmitter* out,
                            const std::string& arg) const
        {
            //static std::string show_nonprintable( std::string in_str ) 
            //{
            char const hex_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', 
                                        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
        
            //for ( int i = 0 ;i < in_str.length() ; i++ ) 
            for ( const char* pos = in; pos < in + inlen; ++pos )
            {
                //char const byte = *pos;
                //u_int posbyte = (u_int)*pos
                
                if( *pos > 31 && *pos < 127 && *pos != 34 ) out->Emit(*pos); //34 = Double Quote
                else 
                {
                    out->Emit('[');
                    out->Emit( hex_chars[ ( *pos & 0xF0 ) >> 4 ] );
                    out->Emit( hex_chars[ ( *pos & 0x0F ) >> 0 ] );
                    out->Emit(']');
                }
                
            }
            //out->Emit(']');
        }
    };


    // CSV escaping as per RFC4180.
    class CSVEscapeModifier : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            bool need_escape = false;

            for ( const char* pos = in; pos < in + inlen; ++pos )
            {
                if ( *pos == '"' || *pos == ',' ||  *pos == ';' ||
                     *pos == '\r' || *pos == '\n' )
                {
                    need_escape = true;
                    break;
                }
            }

            if ( need_escape )
            {
                out->Emit('"');
                for ( const char* pos = in; pos < in + inlen; ++pos )
                {
                    if ( *pos == '"' ){
                        out->Emit('"');
                        out->Emit(*pos);
                    }
                    else if ( *pos == '\n' ){
                        out->Emit('\\');
                        out->Emit('n');
                    }
                    else out->Emit(*pos);

                }
                out->Emit('"');
            }
            else
                out->Emit(in, inlen);
        }
    };

    class IPAddrModifier : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            byte_string b(reinterpret_cast<const unsigned char*>(in), inlen);
            IPAddress addr(b);
            out->Emit(addr.str());
        }
    };

    class IP6AddrModifier : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            byte_string b(reinterpret_cast<const unsigned char*>(in), inlen);
            IPAddress addr(b);
            Tins::IPv6Address addr6 = addr;
            out->Emit(addr6.to_string());
        }
    };

    class IP6AddrBinModifier : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            byte_string b(reinterpret_cast<const unsigned char*>(in), inlen);
            IPAddress addr(b);
            Tins::IPv6Address addr6 = addr;
            for ( auto addrbyte : addr6 )
                out->Emit(addrbyte);
        }
    };

    template<typename T>
    static std::string ToString(const T& val)
    {
        std::ostringstream oss;

        oss << val;
        return oss.str();
    }

    template<>
    std::string ToString(const IPAddress& addr)
    {
        return to_string(addr.asNetworkBinary());
    }

    class IPAddrGeoLocationModifier : public ctemplate::TemplateModifier
    {
    public:
        explicit IPAddrGeoLocationModifier(GeoIPContext& ctx) : ctx_(ctx) {}

        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            byte_string b(reinterpret_cast<const unsigned char*>(in), inlen);
            IPAddress addr(b);
            out->Emit(ToString<uint32_t>(ctx_.location_code(addr)));
        }

    private:
        GeoIPContext& ctx_;
    };

    class IPAddrGeoASNModifier : public ctemplate::TemplateModifier
    {
    public:
        explicit IPAddrGeoASNModifier(GeoIPContext& ctx) : ctx_(ctx) {}

        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            byte_string b(reinterpret_cast<const unsigned char*>(in), inlen);
            IPAddress addr(b);
            out->Emit(ToString<uint32_t>(ctx_.as_number(addr)));
        }

    private:
        GeoIPContext& ctx_;
    };

    class IPAddrGeoASNetmaskModifier : public ctemplate::TemplateModifier
    {
    public:
        explicit IPAddrGeoASNetmaskModifier(GeoIPContext& ctx) : ctx_(ctx) {}

        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            byte_string b(reinterpret_cast<const unsigned char*>(in), inlen);
            IPAddress addr(b);
            out->Emit(ToString<uint16_t>(ctx_.as_netmask(addr)));
        }

    private:
        GeoIPContext& ctx_;
    };

    class NoGeoLocationModifier : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* /* in */, size_t /* inlen */,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* /* out */,
                            const std::string& /* arg */) const
        {
            throw geoip_error("No GeoLocation data.");
        }
    };

    class DateModifier : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            std::string s(in, inlen);
            std::time_t t = static_cast<std::time_t>(std::stoll(s));
            std::tm tm = *std::gmtime(&t);
            char buf[40];
            std::strftime(buf, sizeof(buf), "%F", &tm);
            out->Emit(buf);
        }
    };

    class DateTimeModifier : public ctemplate::TemplateModifier
    {
    public:
        virtual void Modify(const char* in, size_t inlen,
                            const ctemplate::PerExpandData* /* per_expand_data */,
                            ctemplate::ExpandEmitter* out,
                            const std::string& /* arg */) const
        {
            std::string s(in, inlen);
            std::time_t t = static_cast<std::time_t>(std::stoll(s));
            std::tm tm = *std::gmtime(&t);
            char buf[40];
            std::strftime(buf, sizeof(buf), "%F %T", &tm);
            out->Emit(buf);
        }
    };

    CStringModifier cStringModifier;
    HexStringModifier hexStringModifier;
    ShowNonprintable showNonprintable;
    CSVEscapeModifier csvEscapeModifier;
    IPAddrModifier ipaddrModifier;
    IP6AddrModifier ip6addrModifier;
    IP6AddrBinModifier ip6addrBinModifier;
    DateModifier dateModifier;
    DateTimeModifier dateTimeModifier;

    std::unique_ptr<GeoIPContext> geoip;
    std::unique_ptr<IPAddrGeoLocationModifier> ipaddr_geoloc;
    std::unique_ptr<IPAddrGeoASNModifier> ipaddr_geoasn;
    std::unique_ptr<IPAddrGeoASNetmaskModifier> ipaddr_geoasnetmask;
    std::unique_ptr<NoGeoLocationModifier> no_geoloc;

    void load_modifiers(const std::string& db_dir)
    {
        ctemplate::AddModifier("x-cstring", &cStringModifier);
        ctemplate::AddModifier("x-hexstring", &hexStringModifier);
        ctemplate::AddModifier("x-shownonprintable", &showNonprintable);
        ctemplate::AddModifier("x-csvescape", &csvEscapeModifier);
        ctemplate::AddModifier("x-ipaddr", &ipaddrModifier);
        ctemplate::AddModifier("x-ip6addr", &ip6addrModifier);
        ctemplate::AddModifier("x-ip6addr-bin", &ip6addrBinModifier);
        ctemplate::AddModifier("x-date", &dateModifier);
        ctemplate::AddModifier("x-datetime", &dateTimeModifier);

        try
        {
            geoip = make_unique<GeoIPContext>(db_dir);
            ipaddr_geoloc = make_unique<IPAddrGeoLocationModifier>(*geoip);
            ipaddr_geoasn = make_unique<IPAddrGeoASNModifier>(*geoip);
            ipaddr_geoasnetmask = make_unique<IPAddrGeoASNetmaskModifier>(*geoip);
            ctemplate::AddModifier("x-ipaddr-geo-location", ipaddr_geoloc.get());
            ctemplate::AddModifier("x-ipaddr-geo-asn", ipaddr_geoasn.get());
            ctemplate::AddModifier("x-ipaddr-geo-as-netmask", ipaddr_geoasnetmask.get());
        }
        catch (const geoip_error& e)
        {
            LOG_INFO << "No GeoIP data available " << e.what();
            no_geoloc = make_unique<NoGeoLocationModifier>();
            ctemplate::AddModifier("x-ipaddr-geo-location", no_geoloc.get());
            ctemplate::AddModifier("x-ipaddr-geo-asn", no_geoloc.get());
            ctemplate::AddModifier("x-ipaddr-geo-as-netmask", no_geoloc.get());
        }
    }
}

TemplateException::TemplateException(const std::string& fname, const std::string& msg)
    : std::runtime_error("Template " + fname + ": " + msg + ".")
{
}

bool TemplateBackend::loaded_modifiers = false;

TemplateBackend::TemplateBackend(const TemplateBackendOptions& opts, const std::string& fname)
    : OutputBackend(opts.baseopts), opts_(opts)
{
    if ( !loaded_modifiers )
    {
        load_modifiers(opts.geoip_db_dir_path);
        loaded_modifiers = true;
    }

    if ( !ctemplate::LoadTemplate(opts.template_name, ctemplate::DO_NOT_STRIP) )
        throw TemplateLoadException(opts.template_name);

    output_path_ = output_name(fname);

    if ( opts.baseopts.xz_output )
        writer_ = make_unique<XzStreamWriter>(output_path_, opts.baseopts.xz_preset);
    else if ( opts.baseopts.gzip_output )
        writer_ = make_unique<GzipStreamWriter>(output_path_, opts.baseopts.gzip_level);
    else
        writer_ = make_unique<StreamWriter>(output_path_, 0);
}

TemplateBackend::~TemplateBackend()
{
}

void TemplateBackend::output(const QueryResponseData& qr, const Configuration& /* config */)
{
    ctemplate::TemplateDictionary dict("ONE_QUERY_RESPONSE");

    if ( first_line ) {
        dict.ShowSection("QUERY_RESPONSE_HEADER");
        first_line = false;
    }

    dict.SetIntValue("query_response_has_query", !!(qr.qr_flags & block_cbor::HAS_QUERY));
    dict.SetIntValue("query_response_has_response", !!(qr.qr_flags & block_cbor::HAS_RESPONSE));

    if ( qr.qr_flags & block_cbor::HAS_QUERY )
    {
        dict.SetIntValue("query_response_query_has_opt", !!(qr.qr_flags & block_cbor::QUERY_HAS_OPT));
        dict.SetIntValue("query_response_query_has_question", !(qr.qr_flags & block_cbor::QUERY_HAS_NO_QUESTION));
        dict.SetIntValue("query_response_query_has_no_question", !!(qr.qr_flags & block_cbor::QUERY_HAS_NO_QUESTION));
    }

    if ( qr.qr_flags & block_cbor::HAS_RESPONSE )
    {
        dict.SetIntValue("query_response_response_has_opt", !!(qr.qr_flags & block_cbor::RESPONSE_HAS_OPT));
        dict.SetIntValue("query_response_response_has_question", !(qr.qr_flags & block_cbor::RESPONSE_HAS_NO_QUESTION));
        dict.SetIntValue("query_response_response_has_no_question", !!(qr.qr_flags & block_cbor::RESPONSE_HAS_NO_QUESTION));
    }

    if ( qr.qr_transport_flags )
    {
        dict.SetIntValue("transport_tcp", !!(*qr.qr_transport_flags & block_cbor::TCP));
        dict.SetIntValue("transport_ipv6", !!(*qr.qr_transport_flags & block_cbor::IPV6));
    }

    if ( qr.qr_type )
        dict.SetIntValue("transaction_type", *qr.qr_type);

    if ( ( qr.qr_flags & block_cbor::HAS_QUERY ) && qr.dns_flags )
    {
        dict.SetIntValue("query_checking_disabled", !!(*qr.dns_flags & block_cbor::QUERY_CD));
        dict.SetIntValue("query_authenticated_data", !!(*qr.dns_flags & block_cbor::QUERY_AD));
        dict.SetIntValue("query_z", !!(*qr.dns_flags & block_cbor::QUERY_Z));
        dict.SetIntValue("query_recursion_available", !!(*qr.dns_flags & block_cbor::QUERY_RA));
        dict.SetIntValue("query_recursion_desired", !!(*qr.dns_flags & block_cbor::QUERY_RD));
        dict.SetIntValue("query_truncated", !!(*qr.dns_flags & block_cbor::QUERY_TC));
        dict.SetIntValue("query_authoritative_answer", !!(*qr.dns_flags & block_cbor::QUERY_AA));
    }

    if ( qr.query_edns_version )
    {
        dict.SetIntValue("query_edns_version", *qr.query_edns_version);
        if ( ( qr.qr_flags & block_cbor::HAS_QUERY ) && qr.dns_flags )
            dict.SetIntValue("query_do", !!(*qr.dns_flags & block_cbor::QUERY_DO));
    }
    if ( qr.query_edns_payload_size )
        dict.SetIntValue("query_edns_udp_payload_size", *qr.query_edns_payload_size);

    if ( qr.client_hoplimit )
        dict.SetIntValue("client_hoplimit", *qr.client_hoplimit);
    if ( qr.query_size )
        dict.SetIntValue("query_len", *qr.query_size);
    if ( qr.query_opcode )
    {
        dict.SetIntValue("opcode", *qr.query_opcode);
        dict.SetIntValue("query_opcode", *qr.query_opcode);
    }
    if ( qr.query_rcode )
        dict.SetIntValue("query_rcode", *qr.query_rcode);

    if ( qr.qr_flags & block_cbor::HAS_QUERY )
    {
        int qcount = 0;
        if ( !(qr.qr_flags & block_cbor::QUERY_HAS_NO_QUESTION) )
        {
            qcount = 1;
            if ( qr.query_questions )
                qcount += (*qr.query_questions).size();
        }
        dict.SetIntValue("query_qdcount", qcount);
        dict.SetIntValue("query_ancount",
                         ( qr.query_answers )
                         ? (*qr.query_answers).size()
                         : 0);
        dict.SetIntValue("query_nscount",
                         ( qr.query_authorities )
                         ? (*qr.query_authorities).size()
                         : 0);
        qcount = !!(qr.qr_flags & block_cbor::QUERY_HAS_OPT);
        if ( qr.query_additionals )
            qcount += (*qr.query_additionals).size();
        dict.SetIntValue("query_arcount", qcount);
    }

    if ( qr.timestamp )
    {
        dict.SetValue("timestamp_secs", ToString(std::chrono::duration_cast<std::chrono::seconds>((*qr.timestamp).time_since_epoch()).count()));
        dict.SetValue("timestamp_microsecs", ToString(std::chrono::duration_cast<std::chrono::microseconds>((*qr.timestamp).time_since_epoch()).count()));
        dict.SetValue("timestamp_nanosecs", ToString(std::chrono::duration_cast<std::chrono::nanoseconds>((*qr.timestamp).time_since_epoch()).count()));
    }
    if ( qr.response_delay )
        dict.SetIntValue("response_delay_nanosecs", (*qr.response_delay).count());

    if ( qr.id )
        dict.SetIntValue("id", *qr.id);

    if ( qr.client_address )
        dict.SetValue("client_address", ToString(*qr.client_address));
    if ( qr.server_address )
        dict.SetValue("server_address", ToString(*qr.server_address));

    if ( qr.client_port )
        dict.SetIntValue("client_port", *qr.client_port);
    if ( qr.server_port )
        dict.SetIntValue("server_port", *qr.server_port);

    if ( qr.qname )
        dict.SetValue("query_name", ToString(CaptureDNS::decode_domain_name(*qr.qname)));
    if ( qr.query_type )
        dict.SetIntValue("query_type", *qr.query_type);
    if ( qr.query_class )
        dict.SetIntValue("query_class", *qr.query_class);

    if ( ( qr.qr_flags & block_cbor::HAS_RESPONSE ) && qr.dns_flags )
    {
        dict.SetIntValue("response_checking_disabled", !!(*qr.dns_flags & block_cbor::RESPONSE_CD));
        dict.SetIntValue("response_authenticated_data", !!(*qr.dns_flags & block_cbor::RESPONSE_AD));
        dict.SetIntValue("response_z", !!(*qr.dns_flags & block_cbor::RESPONSE_Z));
        dict.SetIntValue("response_recursion_available", !!(*qr.dns_flags & block_cbor::RESPONSE_RA));
        dict.SetIntValue("response_recursion_desired", !!(*qr.dns_flags & block_cbor::RESPONSE_RD));
        dict.SetIntValue("response_truncated", !!(*qr.dns_flags & block_cbor::RESPONSE_TC));
        dict.SetIntValue("response_authoritative_answer", !!(*qr.dns_flags & block_cbor::RESPONSE_AA));
    }

    if ( qr.qr_flags & block_cbor::HAS_RESPONSE )
    {
        int qcount = 0;
        if ( !(qr.qr_flags & block_cbor::RESPONSE_HAS_NO_QUESTION) )
        {
            qcount = 1;
            if ( qr.response_questions )
                qcount += (*qr.response_questions).size();
        }
        dict.SetIntValue("response_qdcount", qcount);
        dict.SetIntValue("response_ancount",
                         ( qr.response_answers )
                         ? (*qr.response_answers).size()
                         : 0);
        dict.SetIntValue("response_nscount",
                         ( qr.response_authorities )
                         ? (*qr.response_authorities).size()
                         : 0);
        dict.SetIntValue("response_arcount",
                         ( qr.response_additionals )
                         ? (*qr.response_additionals).size()
                         : 0);

        bool response_opt = false;
        if ( qr.response_additionals )
            response_opt = std::any_of((*qr.response_additionals).begin(),
                                       (*qr.response_additionals).end(),
                                       [](const QueryResponseData::RR& r)
                                       {
                                           return r.rtype && *r.rtype == CaptureDNS::OPT;
                                       });
        dict.SetIntValue("query_response_response_has_opt", response_opt);
    }

    if ( qr.response_size )
        dict.SetIntValue("response_len", *qr.response_size);

    if ( qr.response_rcode )
        dict.SetIntValue("response_rcode", *qr.response_rcode);

    dict.SetIntValue("query_response_flags", qr.qr_flags);
    if ( qr.qr_transport_flags )
        dict.SetIntValue("transport_flags", *qr.qr_transport_flags);
    if ( qr.dns_flags )
        dict.SetIntValue("dns_flags", *qr.dns_flags);

    for ( auto&& val : opts_.values )
        dict.SetValue(val.first, val.second);

    std::string out;

//      if ( qr.response_answers ) {
//           output << "          Response Answers:    Type    Class    Name\n";
//           for ( const auto& r : *qr.response_answers )
//           {
//               if ( r.rtype )
//                   output << "\t\t             " << std::right << std::setw(6) << Configuration::find_rrtype_string(*r.rtype);
//               if ( r.rclass )
//                   output << "      "<< static_cast<unsigned>(*r.rclass);
//               if ( r.name )
//                   output << "      " << CaptureDNS::decode_domain_name(*r.name) << "\n";

    if ( qr.response_answers ) {
        //dict.SetValue("answer_dname", ToString(CaptureDNS::decode_domain_name(dns->answers().front().dname())) );
        // dict.SetIntValue("answer_length", *qr.response_answers.size())
        
        std::string answertypes; 
        std::string authtypes; 
        std::string addtypes; 

        std::string answerdata;
        std::string authdata;
        std::string adddata;
        
        std::string answername;
        std::string authname;
        std::string addname;

        std::string answerrclass;
        std::string authrclass;
        std::string addrclass;

        std::string answerttl;
        std::string authttl;
        std::string addttl;

        std::string teststr; 
        

                 //answertypes = answertypes + "," + ToString(a.query_type());

        for ( const auto& a : *qr.response_answers ) {
            uint16_t qt = (*a.rtype);
            answertypes = answertypes + "," + Configuration::find_rrtype_string(*a.rtype);
            answerdata = answerdata + "," + CaptureDNS::decode_rdata(*a.rdata) ;
            answername = answername + "," + CaptureDNS::decode_domain_name(*a.name);
            answerrclass = answerrclass + "," + CaptureDNS::textualize_rr_data(qt, *a.rdata) ;;
            //answerrclass = answerrclass + "," + to_string(static_cast<unsigned>(*a.rclass) );
            //answerttl = answerttl + "," + CaptureDNS::decode_domain_name(std.to_string( *a.ttl ) );
        }

        if ( qr.response_authorities) {
            for ( const auto& a : *qr.response_authorities )
            {
               authtypes = authtypes + "," + Configuration::find_rrtype_string(*a.rtype);
               //authdata = authdata + "," + CaptureDNS::decode_domain_name(*a.rdata);
               authname = authname + "," + CaptureDNS::decode_domain_name(*a.name);
            //   authclass = authclass + "," + CaptureDNS::decode_domain_name(*a.class);
            //   authdttl = authttl + "," + CaptureDNS::decode_domain_name(*a.ttl);
            }
        }
        if ( qr.response_additionals ) {
            for ( const auto& a : *qr.response_additionals )
            {
                addtypes = addtypes + "," + Configuration::find_rrtype_string(*a.rtype);
                //adddata = adddata + "," + CaptureDNS::decode_domain_name(*a.rdata);
                addname = addname + "," + CaptureDNS::decode_domain_name(*a.name);
            }
        }
        
        uint8_t slength;
        answertypes.erase(0,1);
        answername.erase(0,1);
        answerdata.erase(0,1);
        answerrclass.erase(0,1);
        answerttl.erase(0,1);
        if ( qr.response_authorities )
        {
            authtypes.erase(0,1);
            authname.erase(0,1);
            authdata.erase(0,1);
        }
        if ( qr.response_additionals )
        {
            addtypes.erase(0,1);
            addname.erase(0,1);
            adddata.erase(0,1);
        }
        
        dict.SetIntValue("answer_count", (*qr.response_answers).size());
        dict.SetValue("answertypes", answertypes);
        dict.SetValue("answername", answername);
        dict.SetValue("answerdata", answerdata);
        dict.SetValue("answerrclass", answerrclass);
        dict.SetValue("answerttl", answerttl);
        if ( qr.response_authorities ) {
            dict.SetIntValue("authority", (*qr.response_authorities).size());
            dict.SetValue("authtypes", authtypes);
            dict.SetValue("authname", authname);
            dict.SetValue("authdata", authdata);
        }
        if ( qr.response_additionals ) {
            dict.SetIntValue("additional", (*qr.response_additionals).size());
            dict.SetValue("addttypes", addtypes);
            dict.SetValue("addname", addname);
            dict.SetValue("adddata", adddata);
        }

//
    }

    if ( ctemplate::ExpandTemplate(opts_.template_name, ctemplate::DO_NOT_STRIP, &dict, &out) )
        writer_->writeBytes(out);
    else
        throw TemplateExpandException(output_path_);
}

std::string TemplateBackend::output_file()
{
    if ( output_path_ == StreamWriter::STDOUT_FILE_NAME )
        return "";
    return output_path_;
}
