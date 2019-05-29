/*
 * Copyright 2016-2019 Internet Corporation for Assigned Names and Numbers.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * Developed by Sinodun IT (www.sinodun.com)
 */

#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include "blockcbordata.hpp"
#include "ipaddress.hpp"

/**
 * \struct Size
 * \brief A value holding a size, an unsigned number that may be specified
 * with a suffix of k, K, m, M, g, G, t, T.
 */
struct Size
{
public:
    Size() : size(0) {}
    explicit Size(std::uintmax_t n) : size(n) {}
    std::uintmax_t size;
};

/**
 * \class Configuration
 * \brief A structure holding configuration settings.
 */
class Configuration
{
public:
    /**
     * \enum OptionalOutputSections
     * \brief Specify which optional output elements in a DNS message are requested.
     */
    enum OptionalOutputSections
    {
        EXTRA_QUESTIONS = 1 << 0,
        ANSWERS = 1 << 1,
        AUTHORITIES = 1 << 2,
        ADDITIONALS = 1 << 3,
        ALL = (EXTRA_QUESTIONS | ANSWERS | AUTHORITIES | ADDITIONALS)
    };

    /**
     * \brief output filename pattern for raw PCAP output.
     *
     * If not empty, all captured packets are written to this file in
     * PCAP format.
     *
     * The filename pattern is run through strftime() to generate the filename.
     */
    std::string raw_pcap_pattern;

    /**
     * \brief output filename pattern for ignored PCAP output.
     *
     * If not empty, all packets that are ignored are written to this file in
     * PCAP format. Packets are currently ignored if they are:
     * - To/From ports other than 53.
     * - Fragmented.
     * - TCP.
     * - Not able to be decoded as well-formed DNS messages.
     *
     * The filename pattern is run through strftime() to generate the filename.
     */
    std::string ignored_pcap_pattern;

    /**
     * \brief output filename pattern for query/response pairs.
     *
     * If not empty, query/response pairs (i.e. the main program output)
     * are written to this file.
     *
     * The filename pattern is run through `strftime()` to generate the filename.
     */
    std::string output_pattern;

    /**
     * \brief compress output data using gzip.
     */
    bool gzip_output;

    /**
     * \brief gzip compression level to use.
     */
    unsigned int gzip_level;

    /**
     * \brief compress output data using xz.
     */
    bool xz_output;

    /**
     * \brief xz compression preset to use.
     */
    unsigned int xz_preset;

    /**
     * \brief compress pcap data using gzip.
     */
    bool gzip_pcap;

    /**
     * \brief pcap gzip compression level to use.
     */
    unsigned int gzip_level_pcap;

    /**
     * \brief compress pcap data using xz.
     */
    bool xz_pcap;

    /**
     * \brief pcap xz compression preset to use.
     */
    unsigned int xz_preset_pcap;

    /**
     * \brief maximum number of compression threads.
     */
    unsigned int max_compression_threads;

    /**
     * \brief rotation period for all output files, in seconds.
     */
    unsigned int rotation_period;

    /**
     * \brief period in seconds after which a query is deemed to
     * not have received a response.
     */
    unsigned int query_timeout;

    /**
     * \brief the maximum time in microseconds to allow for out of
     * temporal order packet delivery. If a response arrives without a
     * query, once a packet arrives with a timestamp this much later,
     * give up hoping for a query to arrive.
     */
    unsigned int skew_timeout;

    /**
     * \brief packet capture snap length. See `tcpdump` documentation for more.
     */
    unsigned int snaplen;

    /**
     * \brief `true` if the interface should be put into promiscous mode.
     * See `tcpdump` documentation for more.
     */
    bool promisc_mode;

    /**
     * \brief the network interfaces to capture from.
     *
     * This will be operating system dependent. A Linux example is `eth0`.
     */
    std::vector<std::string> network_interfaces;

    /**
     * \brief the server network addresses.
     *
     * Optional addresses for the server interfaces. Stored in C-DNS but
     * not otherwise used.
     */
    std::vector<IPAddress> server_addresses;

    /**
     * \brief packet filter
     *
     * `libpcap` packet filter expression. Packets not matching will be
     * silently discarded.
     */
    std::string filter;

    /**
     * \brief what sections of query messages are to be output.
     *
     * This is a mask of ORd OptionalOutputSections values.
     */
    int output_options_queries;

    /**
     * \brief what sections of response messages are to be output.
     *
     * This is a mask of ORd OptionalOutputSections values.
     */
    int output_options_responses;

    /**
     * \brief which vlan IDs are to be accepted.
     */
    std::vector<unsigned> vlan_ids;

    /**
     * \brief which opcodes are to be ignored on output.
     */
    std::vector<unsigned> ignore_opcodes;

    /**
     * \brief which opcodes are to be included on output.
     */
    std::vector<unsigned> accept_opcodes;

    /**
     * \brief which RR types are to be ignored on output.
     */
    std::vector<unsigned> ignore_rr_types;

    /**
     * \brief which RR types are to be included on output.
     */
    std::vector<unsigned> accept_rr_types;

    /**
     * \brief set the maximum number of query/response items
     * or address event items in a block.
     */
    unsigned int max_block_items;

    /**
     * \brief set the maximum uncompressed output size. 0 = no limit.
     */
    Size max_output_size;

    /**
     * \brief report statistics on exit
     */
    bool report_info;

    /**
     * \brief log stats periodically during packet collection
     */
    unsigned int log_network_stats_period;

    /**
     * \brief output text summary of individual DNS messages.
     */
    bool debug_dns;

    /**
     * \brief output text summary of individual query/response pairs.
     */
    bool debug_qr;

    /**
     * \brief don't write host identifier info to CBOR output.
     *
     * This is not a command line parameter, but driven from
     * whether the input is a PCAP.
     */
    bool omit_hostid;

    /**
     * \brief don't write system identifier infos to CBOR output.
     *
     * This is a hidden debug parameter.
     */
    bool omit_sysid;

    /**
     * \brief set the maximum size of the inter-thread channels.
     *
     * This is (for now) a hidden debug parameter.
     */
    unsigned int max_channel_size;

    /**
     * \brief size of address prefix stored for client IPv4 addresses.
     *
     * This is not an exposed option, but picked up from format 1.0
     * block parameters.
     */
    unsigned int client_address_prefix_ipv4;

    /**
     * \brief size of address prefix stored for client IPv6 addresses.
     *
     * This is not an exposed option, but picked up from format 1.0
     * block parameters.
     */
    unsigned int client_address_prefix_ipv6;

    /**
     * \brief size of address prefix stored for server IPv4 addresses.
     *
     * This is not an exposed option, but picked up from format 1.0
     * block parameters.
     */
    unsigned int server_address_prefix_ipv4;

    /**
     * \brief size of address prefix stored for server IPv6 addresses.
     *
     * This is not an exposed option, but picked up from format 1.0
     * block parameters.
     */
    unsigned int server_address_prefix_ipv6;

    /**
     * \brief Default constructor.
     */
    Configuration();

    /**
     * \brief Parse command line.
     *
     * This parses the command line and the configuration file, if present.
     *
     * \param ac number of arguments.
     * \param av argument vector.
     * \return variable map with the results.
     * \throws boost::program_options::error on error.
     */
    boost::program_options::variables_map parse_command_line(int ac, char *av[]);

    /**
     * \brief Re-read the configuration file.
     *
     * \throws boost::program_options::error on error.
     */
    boost::program_options::variables_map reread_config_file();

    /**
     * \brief Return the options for usage output.
     *
     * \returns options description.
     */
    std::string options_usage() const;

    /**
     * \brief Dump the configuration to the stream provided
     *
     * \param os output stream.
     */
    void dump_config(std::ostream& os) const;

    /**
     * \brief Determine whether a particular OPCODE should be output.
     *
     * Check the OPCODE against the list of configured accept and ignore
     * OPCODEs.
     *
     * \param opcode the OPCODE.
     * \returns `true` if it should be output.
     */
    bool output_opcode(CaptureDNS::Opcode opcode) const;

    /**
     * \brief Determine whether a particular RR type should be output.
     *
     * Check the RR type against the list of configured accept and ignore
     * RR types.
     *
     * \param rr_type the RR type.
     * \returns `true` if it should be output.
     */
    bool output_rr_type(CaptureDNS::QueryType rr_type) const;

    /**
     * \brief Populate BlockParameters instance from config.
     *
     * Populate a BlockParameters instance using current configuration
     * settings.
     *
     * \param bp        block parameters.
     */
    void populate_block_parameters(block_cbor::BlockParameters& bp) const;

    /**
     * \brief Populate config from BlockParameters instance.
     *
     * \param bp        block parameters.
     */
    void set_from_block_parameters(const block_cbor::BlockParameters& bp);

protected:
    /**
     * \brief Set configuration items that aren't directly set by Boost.
     */
    void set_config_items(const boost::program_options::variables_map& vm);

private:
    /**
     * \brief the configuration file, if any.
     */
    std::string config_file_;

    /**
     * \brief variable map from command line only parse.
     */
    boost::program_options::variables_map cmdline_vars_;

    /**
     * \brief Command line only visible options.
     */
    boost::program_options::options_description cmdline_options_;

    /**
     * \brief Command line only hidden options.
     */
    boost::program_options::options_description cmdline_hidden_options_;

    /**
     * \brief Configuration file options.
     */
    boost::program_options::options_description config_file_options_;

    /**
     * \brief Positional options.
     */
    boost::program_options::positional_options_description positional_options_;
    
    /**
     * \brief Helper method to print output options
     */
    void dump_output_option(std::ostream& os, bool query) const;

    /**
     * \brief Helper method to print OPCODEs
     */
    void dump_OPCODEs(std::ostream& os, bool accept) const;

    /**
     * \brief Helper method to print RR types
     */
    void dump_RR_types(std::ostream& os, bool accept) const;
};

#endif
