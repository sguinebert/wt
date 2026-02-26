/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * All rights reserved.
 */

#include "Wt/WConfig.h"
#include "Wt/WLogger.h"
#include "Wt/WServer.h"

#include "Configuration.h"
#include "WebUtils.h"
#include "StringUtils.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WT_WIN32
#include <unistd.h>
#endif
#ifdef WT_WIN32
#include <process.h> // for getpid()
#include <winsock2.h> // for gethostname()
#endif

//#include <boost/program_options.hpp>
#include <Wt/CLI11.hpp>
#include <algorithm>
#include <iostream>
#include <fstream>

#ifdef __CYGWIN__
#include <winsock2.h> // for gethostname()
#endif

namespace Wt {
LOGGER("wthttp");
}

namespace Http {
namespace server {

// void Configuration::setOptions(const std::string &applicationPath,
//                                const std::vector<std::string> &args,
//                                const std::string &configurationFile)
// {
//   po::options_description all_options("Allowed options");
//   po::options_description visible_options("Allowed options");
//   createOptions(all_options, visible_options);

//   try {
//     po::variables_map vm;

//     if (!args.empty())
//       po::store(po::command_line_parser(args).options(all_options).allow_unregistered().run(), vm);

//     if (!configurationFile.empty()) {
//       std::ifstream cfgFile(configurationFile.c_str(),
//         std::ios::in | std::ios::binary);
//       if (cfgFile) {
//         if (!silent_)
//           LOG_INFO_S(this, "reading wthttpd configuration from: {}", configurationFile);
//         po::store(po::parse_config_file(cfgFile, all_options), vm);
//       }
//     }

//     po::notify(vm);

//     if (vm.count("help")) {
//       std::cout << visible_options << std::endl;

//       if (!configurationFile.empty())
//         std::cout << "Settings may be set in the configuration file " << configurationFile << std::endl;

//       std::cout << std::endl;

//       throw Wt::WServer::Exception("");
//     }

//     readOptions(vm);
//   } catch (Wt::WServer::Exception& e) {
//     throw;
//   } catch (std::exception& e) {
//     throw Wt::WServer::Exception(std::string("Error: ") + e.what());
//   } catch (...) {
//     throw Wt::WServer::Exception("Exception of unknown type!\n");
//   }

//   options_.clear();
//   options_.push_back(applicationPath);
//   options_.insert(options_.end(), args.begin(), args.end());
// }

// void Configuration::createOptions(po::options_description &options, po::options_description &visible_options)
// {
//   po::options_description general("General options");
//   general.add_options()
//       ("help,h", "produce help message")

//       ("threads,t",
//        po::value<int>(&threads_)->default_value(threads_),
//        "number of threads (-1 indicates that num_threads from wt_config.xml "
//        "is to be used, which defaults to 10)")

//       ("servername",
//        po::value<std::string>(&serverName_)->default_value(serverName_),
//        "servername (IP address or DNS name)")

//       ("docroot",
//        po::value<std::string>()->default_value(docRoot_),
//        "document root for static files, optionally followed by a "
//        "comma-separated list of paths with static files (even if they "
//        "are within a deployment path), after a ';' \n\n"
//        "e.g. --docroot=\".;/favicon.ico,/resources,/style\"\n")

//       ("resources-dir",
//        po::value<std::string>(&resourcesDir_)->default_value(resourcesDir_),
//        "path to the Wt resources folder. By default, Wt will look for its resources "
//        "in the resources subfolder of the docroot (see --docroot). If a file is not found "
//        "in that resources folder, this folder will be checked instead as a fallback. "
//        "If this option is omitted, then Wt will not use a fallback resources folder."
//        )

//       ("approot",
//        po::value<std::string>(&appRoot_)->default_value(appRoot_),
//        "application root for private support files; if unspecified, the value "
//        "of the environment variable $WT_APP_ROOT is used, "
//        "or else the current working directory")

//       ("errroot",
//        po::value<std::string>(&errRoot_)->default_value(errRoot_),
//        "root for error pages")

//       ("accesslog",
//        po::value<std::string>(&accessLog_),
//        "access log file (defaults to stdout), "
//        "to disable access logging completely, use --accesslog=-")

//       ("no-compression",
//        "do not use compression")

//       ("deploy-path",
//        po::value<std::string>(&deployPath_)->default_value(deployPath_),
//        "location for deployment")

//       ("session-id-prefix",
//        po::value<std::string>(&sessionIdPrefix_)->default_value(sessionIdPrefix_),
//        "prefix for session IDs (overrides wt_config.xml setting)")

//       ("pid-file,p",
//        po::value<std::string>(&pidPath_)->default_value(pidPath_),
//        "path to pid file (optional)")

//       ("config,c",
//        po::value<std::string>(&configPath_),
//        ("location of wt_config.xml; if unspecified, the value of the environment "
//         "variable $WT_CONFIG_XML is used, or else the built-in default "
//         "(" + std::string(WT_CONFIG_XML) + ") is tried, or else built-in "
//                                        "defaults are used").c_str())

//       ("max-memory-request-size",
//        po::value< ::int64_t >(&maxMemoryRequestSize_)
//            ->default_value(maxMemoryRequestSize_),
//        "threshold for request size (bytes), for spooling the entire request to "
//        "disk, to avoid DoS")

//       ("gdb",
//        "do not shutdown when receiving Ctrl-C (and let gdb break instead)")
//       ;

//   po::options_description http("HTTP/WebSocket server options");
//   http.add_options()
//       ("http-listen", po::value<std::vector<std::string> >(&httpListen_)->multitoken(),
//        "address/port pair to listen on. If no port is specified, 80 is used as the default, e.g. "
//        "127.0.0.1:8080 will cause the server to listen on port 8080 of 127.0.0.1 (localhost). "
//        "For IPv6, use square brackets, e.g. [::1]:8080 will cause the server to listen on port "
//        "8080 of [::1] (localhost). This argument can be repeated, e.g. "
//        "--http-listen 0.0.0.0:8080 --http-listen [0::0]:8080 will cause the server to listen on "
//        "port 8080 of all interfaces using IPv4 and IPv6. You must specify this option or --https-listen "
//        "at least once. The older style --http-address and --https-address can also be used for backwards "
//        "compatibility."
// #ifndef NO_RESOLVE_ACCEPT_ADDRESS
//        " "
//        "If a hostname is provided instead of an IP address, the server "
//        "will listen on all of the addresses (IPv4 and IPv6) that this hostname resolves to."
// #endif // NO_RESOLVE_ACCEPT_ADDRESS
//        )
//       ("http-address", po::value<std::string>(),
//        "IPv4 (e.g. 0.0.0.0) or IPv6 Address (e.g. 0::0). You must specify either "
//        "--http-listen, --https-listen, --http-address, or --https-address.")
//       ("http-port", po::value<std::string>(&httpPort_)->default_value(httpPort_),
//        "HTTP port (e.g. 80)")
//       ;

//   po::options_description https("HTTPS/Secure WebSocket server options");
//   https.add_options()
//       ("https-listen", po::value<std::vector<std::string> >(&httpsListen_)->multitoken(),
//        "address/port pair to listen on. If no port is specified, 80 is used as the default, e.g. "
//        "127.0.0.1:8080 will cause the server to listen on port 8080 of 127.0.0.1 (localhost). "
//        "For IPv6, use square brackets, e.g. [::1]:8080 will cause the server to listen on port "
//        "8080 of [::1] (localhost). This argument can be repeated, e.g. "
//        "--https-listen 0.0.0.0:8080 --https-listen [0::0]:8080 will cause the server to listen on "
//        "port 8080 of all interfaces using IPv4 and IPv6."
// #ifndef NO_RESOLVE_ACCEPT_ADDRESS
//        " "
//        "If a hostname is provided instead of an IP address, the server "
//        "will listen on all of the addresses (IPv4 and IPv6) that this hostname resolves to."
// #endif // NO_RESOLVE_ACCEPT_ADDRESS
//        )
//       ("https-address", po::value<std::string>(),
//        "IPv4 (e.g. 0.0.0.0) or IPv6 Address (e.g. 0::0). You must specify either "
//        "--http-listen, --https-listen, --http-address, or --https-address.")
//       ("https-port",
//        po::value<std::string>(&httpsPort_)->default_value(httpsPort_),
//        "HTTPS port (e.g. 443)")
//       ("ssl-certificate",
//        po::value<std::string>()->default_value(sslCertificateChainFile_),
//        "SSL server certificate chain file\n"
//        "e.g. \"/etc/ssl/certs/vsign1.pem\"")
//       ("ssl-private-key", po::value<std::string>()->default_value(sslPrivateKeyFile_),
//        "SSL server private key file\n"
//        "e.g. \"/etc/ssl/private/company.pem\"")
//       ("ssl-tmp-dh", po::value<std::string>()->default_value(sslTmpDHFile_),
//        "File for temporary Diffie-Hellman parameters\n"
//        "e.g. \"/etc/ssl/dh512.pem\"")
//       ("ssl-enable-v3",
//        "Switch on SSLv3 support (not recommended; disabled by default)")
//       ("ssl-client-verification",
//        po::value<std::string>(&sslClientVerification_)
//            ->default_value(sslClientVerification_),
//        "The verification mode for client certificates.\n"
//        "This is either 'none', 'optional' or 'required'. When 'none', the server "
//        "will not request a client certificate. When 'optional', the server will "
//        "request a certificate, but the client does not have to supply one. With "
//        "'required', the connection will be terminated if the client does not "
//        "provide a valid certificate.")
//       ("ssl-verify-depth",
//        po::value<int>(&sslVerifyDepth_)->default_value(sslVerifyDepth_),
//        "Specifies the maximum length of the server certificate chain.\n")
//       ("ssl-ca-certificates",
//        po::value<std::string>(&sslCaCertificates_)
//            ->default_value(sslCaCertificates_),
//        "Path to a file containing the concatenated trusted CA certificates, "
//        "which can be used to authenticate the client. The file should contains a "
//        "a number of PEM-encoded certificates.\n")
//       ("ssl-cipherlist",
//        po::value<std::string>(&sslCipherList_)
//            ->default_value(sslCipherList_),
//        "List of acceptable ciphers for SSL. This list is passed as-is to the SSL "
//        "layer, so see openssl for the proper syntax. When empty, the default "
//        "acceptable cipher list will be used. Example cipher list string: "
//        "\"TLSv1+HIGH:!SSLv2\"\n")
//       ("ssl-prefer-server-ciphers",
//        po::value<bool>(&sslPreferServerCiphers_)
//            ->default_value(sslPreferServerCiphers_),
//        "By default, the client's preference is used for determining the cipher "
//        "that is choosen during a SSL or TLS handshake. By enabling this option, "
//        "the server's preference will be used." )
//       ;

//   po::options_description hidden("Hidden options");
//   hidden.add_options()
//       ("parent-port", po::value<int>(&parentPort_)->default_value(parentPort_));

//   options.add(general).add(http).add(https).add(hidden);
//   visible_options.add(general).add(http).add(https);
// }

// void Configuration::readOptions(const po::variables_map &vm)
// {
//   if (!pidPath_.empty() && parentPort_ == -1) {
//     std::ofstream pidFile(pidPath_.c_str());

//     if (!pidFile)
//       throw Wt::WServer::Exception("Cannot write to '" + pidPath_ + "'");

//     pidFile << getpid() << std::endl;
//   }

//   gdb_ = vm.count("gdb");

//   compression_ = !vm.count("no-compression");
// #ifndef WTHTTP_WITH_ZLIB
//   if(compression_) {
//     std::cout << "Option no-compression is implied because wthttp was built "
//               << "without zlib support.\n";
//     compression_ = false;
//   }
// #endif

//   if (vm.count("docroot")) {
//     docRoot_ = vm["docroot"].as<std::string>();

//     if (docRoot_ == "") {
//       throw Wt::WServer::Exception(
//           "Document root was not set, or was set to the empty path. "
//           "Use --docroot to set the HTML root directory.");
//     }
//     Wt::Utils::SplitVector parts;
//     boost::split(parts, docRoot_, boost::is_any_of(";"));

//     if (parts.size() > 1) {
//       if (parts.size() != 2)
//         throw Wt::WServer::Exception("Document root (--docroot) should be "
//                                      "of format path[;./p1[,p2[,...]]]");
//       boost::split(staticPaths_, parts[1], boost::is_any_of(","));
//       staticPaths_.erase(std::remove(staticPaths_.begin(), staticPaths_.end(), ""), staticPaths_.end());
//       defaultStatic_ = false;
//     }

//     if (parts.size() > 0)
//       docRoot_ = std::string(parts[0].begin(), parts[0].end());

//     checkPath(docRoot_, "Document root", Directory);
//   } else
//     throw Wt::WServer::Exception("Document root (--docroot) was not set.");

//   if (vm.count("http-address"))
//     httpAddress_ = vm["http-address"].as<std::string>();

//   if (errRoot_.empty()) {
//     errRoot_ = docRoot_;
//     if (!errRoot_.empty()) {
//       if (errRoot_[errRoot_.length()-1] != '/')
//         errRoot_+= '/';
//     }
//     errRoot_ += "error/";
//   }
//   if (errRoot_[errRoot_.length()-1] != '/')
//     errRoot_+= '/';

//   if (deployPath_.empty())
//     deployPath_ = "/";
//   else
//     if (deployPath_[0] != '/')
//       throw Wt::WServer::Exception("Deployment root must start with '/'");

//   sslEnableV3_ = vm.count("ssl-enable-v3");

//   if (vm.count("https-address")) {
//     httpsAddress_ = vm["https-address"].as<std::string>();
//   }

//   if (vm.count("https-listen") || vm.count("https-address")) {
//     checkPath(vm, "ssl-certificate", "SSL Certificate chain file",
//               sslCertificateChainFile_, RegularFile);
//     checkPath(vm, "ssl-private-key", "SSL Private key file",
//               sslPrivateKeyFile_, RegularFile | Private);
//     checkPath(vm, "ssl-tmp-dh", "SSL Temporary Diffie-Hellman file",
//               sslTmpDHFile_, RegularFile);
//   }

//   if (sslClientVerification_ != "none") {

//     checkPath(vm, "ssl-ca-certificates",
//               "Client authentication SSL CA certificates file",
//               sslCaCertificates_, RegularFile);

//     if (sslClientVerification_ != "optional" &&
//         sslClientVerification_ != "once" &&
//         sslClientVerification_ != "required") {
//       throw Wt::WServer::Exception(
//           "ssl-client-verification must be \"none\", \"optional\", \"once\" or "
//           "\"required\"");
//     }
//   }

//   if (httpListen_.empty() &&
//       httpAddress_.empty() &&
//       httpsListen_.empty() &&
//       httpsAddress_.empty()) {
//     throw Wt::WServer::Exception
//         ("Specify http-listen, https-listen, http-address and/or https-address "
//          "to run a HTTP and/or HTTPS server.");
//   }
// }


void Configuration::setOptions(const std::string &applicationPath,
                               const std::vector<std::string> &args,
                               const std::string &configurationFile)
{

    std::cerr << "args vus par Configuration::setOptions:\n";
    for (auto &s : args)
        std::cerr << "  [" << s << "]\n";


    std::string docrootOption;

    bool noCompressionFlag = false;

    try {
        CLI::App app{"Wt HTTP server"};

        // Qt / Wt utilisait déjà -h/--help, on garde la même sémantique
        app.set_help_flag("-h,--help", "produce help message");

        // ----- General options -----
        app.add_option("-t,--threads", threads_,
                       "number of threads (-1 indicates that num_threads from "
                       "wt_config.xml is to be used, which defaults to 10)")
            ->capture_default_str();

        app.add_option("--servername", serverName_,
                       "servername (IP address or DNS name)")
            ->capture_default_str();

        // docroot : on la rend "required" au lieu du jeu bizarre default+test vide
        auto optDocroot =
            app.add_option("--docroot", docrootOption,
                           "document root for static files, optionally followed by a "
                           "comma-separated list of paths with static files (even if"
                           " they are within a deployment path), after a ';' \n\n"
                           "e.g. --docroot=\".;/favicon.ico,/resources,/style\"\n")
                ->required();

        app.add_option("--resources-dir", resourcesDir_,
                       "path to the Wt resources folder. By default, Wt will look "
                       "for its resources in the resources subfolder of the "
                       "docroot (see --docroot). If a file is not found in that "
                       "resources folder, this folder will be checked instead as a "
                       "fallback. If this option is omitted, then Wt will not use "
                       "a fallback resources folder.")
            ->capture_default_str();

        app.add_option("--approot", appRoot_,
                       "application root for private support files; if unspecified, "
                       "the value of the environment variable $WT_APP_ROOT is used, "
                       "or else the current working directory")
            ->capture_default_str();

        app.add_option("--errroot", errRoot_,
                       "root for error pages")
            ->capture_default_str();

        app.add_option("--accesslog", accessLog_,
                       "access log file (defaults to stdout), "
                       "to disable access logging completely, use --accesslog=-");

        app.add_flag("--no-compression", noCompressionFlag,
                     "do not use compression");

        app.add_option("--deploy-path", deployPath_,
                       "location for deployment")
            ->capture_default_str();

        app.add_option("--session-id-prefix", sessionIdPrefix_,
                       "prefix for session IDs (overrides wt_config.xml setting)")
            ->capture_default_str();

        app.add_option("-p,--pid-file", pidPath_,
                       "path to pid file (optional)")
            ->capture_default_str();

        app.add_option("-c,--config", configPath_,
                       std::string("location of wt_config.xml; if unspecified, the "
                                   "value of the environment variable $WT_CONFIG_XML "
                                   "is used, or else the built-in default (") +
                           WT_CONFIG_XML +
                           ") is tried, or else built-in defaults are used")
            ->capture_default_str();

        app.add_option("--max-memory-request-size", maxMemoryRequestSize_,
                       "threshold for request size (bytes), for spooling the entire "
                       "request to disk, to avoid DoS")
            ->capture_default_str();

        app.add_flag("--gdb", gdb_,
                     "do not shutdown when receiving Ctrl-C (and let gdb break instead)");

        // ----- HTTP options -----
        app.add_option("--http-listen", httpListen_,
                       "address/port pair to listen on. If no port is specified, 80 "
                       "is used as the default, e.g. 127.0.0.1:8080 will cause the "
                       "server to listen on port 8080 of 127.0.0.1 (localhost). "
                       "For IPv6, use square brackets, e.g. [::1]:8080 will cause "
                       "the server to listen on port 8080 of [::1] (localhost). "
                       "This argument can be repeated.\n"
#ifndef NO_RESOLVE_ACCEPT_ADDRESS
                       "If a hostname is provided instead of an IP address, the server "
                       "will listen on all of the addresses (IPv4 and IPv6) that this "
                       "hostname resolves to."
#endif
                       )
            ->expected(-1); // vector<string>

        app.add_option("--http-address", httpAddress_,
                       "IPv4 (e.g. 0.0.0.0) or IPv6 Address (e.g. 0::0). You must "
                       "specify either --http-listen, --https-listen, --http-address, "
                       "or --https-address.");

        app.add_option("--http-port", httpPort_,
                       "HTTP port (e.g. 80)")
            ->capture_default_str();

        // ----- HTTPS options -----
        app.add_option("--https-listen", httpsListen_,
                       "address/port pair to listen on. If no port is specified, 80 "
                       "is used as the default, e.g. 127.0.0.1:8080 ...")
            ->expected(-1);

        app.add_option("--https-address", httpsAddress_,
                       "IPv4 (e.g. 0.0.0.0) or IPv6 Address (e.g. 0::0). You must "
                       "specify either --http-listen, --https-listen, --http-address, "
                       "or --https-address.");

        app.add_option("--https-port", httpsPort_,
                       "HTTPS port (e.g. 443)")
            ->capture_default_str();

        app.add_option("--ssl-certificate", sslCertificateChainFile_,
                       "SSL server certificate chain file\n"
                       "e.g. \"/etc/ssl/certs/vsign1.pem\"")
            ->capture_default_str();

        app.add_option("--ssl-private-key", sslPrivateKeyFile_,
                       "SSL server private key file\n"
                       "e.g. \"/etc/ssl/private/company.pem\"")
            ->capture_default_str();

        app.add_option("--ssl-tmp-dh", sslTmpDHFile_,
                       "File for temporary Diffie-Hellman parameters\n"
                       "e.g. \"/etc/ssl/dh512.pem\"")
            ->capture_default_str();

        app.add_flag("--ssl-enable-v3", sslEnableV3_,
                     "Switch on SSLv3 support (not recommended; disabled by default)");

        app.add_option("--ssl-client-verification", sslClientVerification_,
                       "The verification mode for client certificates.\n"
                       "This is either 'none', 'optional' or 'required'.")
            ->capture_default_str();

        app.add_option("--ssl-verify-depth", sslVerifyDepth_,
                       "Specifies the maximum length of the server certificate chain.\n")
            ->capture_default_str();

        app.add_option("--ssl-ca-certificates", sslCaCertificates_,
                       "Path to a file containing the concatenated trusted CA certificates, "
                       "which can be used to authenticate the client.")
            ->capture_default_str();

        app.add_option("--ssl-cipherlist", sslCipherList_,
                       "List of acceptable ciphers for SSL. Passed as-is to OpenSSL.")
            ->capture_default_str();

        app.add_option("--ssl-prefer-server-ciphers", sslPreferServerCiphers_,
                       "Use the server's cipher preference instead of the client's.")
            ->capture_default_str();

        // ----- Hidden option -----
        app.add_option("--parent-port", parentPort_, "internal parent port")
            ->group(""); // groupe vide = caché dans le help

        // ----- Autoriser des options non reconnues (comme allow_unregistered) -----
        app.allow_extras(true);

        // ----- Support fichier de config externe (configurationFile param) -----
        CLI::Option *cfgOpt = nullptr;
        if (!configurationFile.empty()) {
            cfgOpt = app.set_config("--wthttpd-config", "",
                                    "wthttpd configuration file", false);
            cfgOpt->group(""); // caché dans le help
            app.allow_config_extras(CLI::config_extras_mode::ignore);
        }

        // Construire un argv artificiel : [applicationPath] + args + éventuellement --wthttpd-config
        std::vector<std::string> cliArgs;
        cliArgs.reserve(args.size() + 3);
        cliArgs.push_back(applicationPath);
        cliArgs.insert(cliArgs.end(), args.begin(), args.end());
        if (cfgOpt && !configurationFile.empty()) {
            cliArgs.push_back("--wthttpd-config");
            cliArgs.push_back(configurationFile);
        }

        try {
            app.parse(cliArgs);
        } catch (const CLI::CallForHelp &e) {
            std::cout << app.help() << std::endl;
            if (!configurationFile.empty()) {
                std::cout << "Settings may be set in the configuration file "
                          << configurationFile << std::endl << std::endl;
            }
            throw Wt::WServer::Exception("");
        } catch (const CLI::ParseError &e) {
            throw Wt::WServer::Exception(std::string("Error: ") + e.what());
        }

        // ---------- Post-traitement : ancien readOptions(vm) réécrit ----------

        // compression
        compression_ = !noCompressionFlag;
#ifndef WTHTTP_WITH_ZLIB
        if (compression_) {
            std::cout << "Option no-compression is implied because wthttp was built "
                      << "without zlib support.\n";
            compression_ = false;
        }
#endif

        // pid-file
        if (!pidPath_.empty() && parentPort_ == -1) {
            std::ofstream pidFile(pidPath_.c_str());
            if (!pidFile)
                throw Wt::WServer::Exception("Cannot write to '" + pidPath_ + "'");
            pidFile << getpid() << std::endl;
        }

        // docroot : parse "path;./p1,p2,..."
        if (docrootOption.empty()) {
            throw Wt::WServer::Exception(
                "Document root was not set, or was set to the empty path. "
                "Use --docroot to set the HTML root directory.");
        }

        Wt::Utils::SplitVector parts;
        boost::split(parts, docrootOption, boost::is_any_of(";"));

        if (parts.size() > 1) {
            if (parts.size() != 2)
                throw Wt::WServer::Exception("Document root (--docroot) should be "
                                             "of format path[;./p1[,p2[,...]]]");
            boost::split(staticPaths_, parts[1], boost::is_any_of(","));
            staticPaths_.erase(std::remove(staticPaths_.begin(),
                                           staticPaths_.end(), ""),
                               staticPaths_.end());
            defaultStatic_ = false;
        }

        if (!parts.empty())
            docRoot_ = std::string(parts[0].begin(), parts[0].end());

        checkPath(docRoot_, "Document root", Directory);

        // http-address: déjà dans httpAddress_ si fourni

        // errRoot_ par défaut = docRoot_/error/
        if (errRoot_.empty()) {
            errRoot_ = docRoot_;
            if (!errRoot_.empty() && errRoot_.back() != '/')
                errRoot_ += '/';
            errRoot_ += "error/";
        }
        if (!errRoot_.empty() && errRoot_.back() != '/')
            errRoot_ += '/';

        // deployPath_
        if (deployPath_.empty()) {
            deployPath_ = "/";
        } else if (deployPath_.front() != '/') {
            throw Wt::WServer::Exception("Deployment root must start with '/'");
        }

        // sslEnableV3_ déjà mis par le flag

        // https-address déjà dans httpsAddress_

        // SSL files obligatoires si HTTPS actif
        if (!httpsListen_.empty() || !httpsAddress_.empty()) {
            if (sslCertificateChainFile_.empty())
                throw Wt::WServer::Exception("SSL Certificate chain file (--ssl-certificate) was not set.");
            checkPath(sslCertificateChainFile_,
                      "SSL Certificate chain file",
                      RegularFile);

            if (sslPrivateKeyFile_.empty())
                throw Wt::WServer::Exception("SSL Private key file (--ssl-private-key) was not set.");
            checkPath(sslPrivateKeyFile_,
                      "SSL Private key file",
                      RegularFile | Private);

            if (sslTmpDHFile_.empty())
                throw Wt::WServer::Exception("SSL Temporary Diffie-Hellman file (--ssl-tmp-dh) was not set.");
            checkPath(sslTmpDHFile_,
                      "SSL Temporary Diffie-Hellman file",
                      RegularFile);
        }

        if (sslClientVerification_ != "none") {
            if (sslCaCertificates_.empty())
                throw Wt::WServer::Exception(
                    "Client authentication SSL CA certificates file "
                    "(--ssl-ca-certificates) was not set.");

            checkPath(sslCaCertificates_,
                      "Client authentication SSL CA certificates file",
                      RegularFile);

            if (sslClientVerification_ != "optional" &&
                sslClientVerification_ != "once" &&
                sslClientVerification_ != "required") {
                throw Wt::WServer::Exception(
                    "ssl-client-verification must be \"none\", \"optional\", \"once\" or "
                    "\"required\"");
            }
        }

        if (httpListen_.empty() &&
            httpAddress_.empty() &&
            httpsListen_.empty() &&
            httpsAddress_.empty()) {
            throw Wt::WServer::Exception
                ("Specify http-listen, https-listen, http-address and/or https-address "
                 "to run a HTTP and/or HTTPS server.");
        }

    } catch (Wt::WServer::Exception&) {
        throw;
    } catch (std::exception& e) {
        throw Wt::WServer::Exception(std::string("Error: ") + e.what());
    } catch (...) {
        throw Wt::WServer::Exception("Exception of unknown type!\n");
    }

    options_.clear();
    options_.push_back(applicationPath);
    options_.insert(options_.end(), args.begin(), args.end());
}


Wt::WLogEntry Configuration::log(const std::string& type) const
{
    Wt::WLogEntry e = logger_.entry(type);

    e << Wt::WLogger::timestamp << Wt::WLogger::sep
      << getpid() << Wt::WLogger::sep
      << /* sessionId << */ Wt::WLogger::sep
      << '[' << type << ']' << Wt::WLogger::sep;

    return e;
}

#ifdef _MSC_VER
static inline bool S_ISREG(unsigned short mode)
{
    return (mode & S_IFREG) != 0;
}

static inline bool S_ISDIR(unsigned short mode)
{
    return (mode & S_IFDIR) != 0;
}
#endif

// void Configuration::checkPath(const po::variables_map& vm,
//                               std::string varName,
//                               std::string varDescription,
//                               std::string& result,
//                               int options)
// {
//   if (vm.count(varName)) {
//     result = vm[varName].as<std::string>();

//     checkPath(result, varDescription, options);
//   } else {
//     throw Wt::WServer::Exception(varDescription + " (--" + varName
//                                  + ") was not set.");
//   }
// }

void Configuration::checkPath(std::string& result,
                              std::string varDescription,
                              int options)
{
    struct stat t;
    if (stat(result.c_str(), &t) != 0) {
        std::perror("stat");
        throw Wt::WServer::Exception(varDescription
                                     + " (\"" + result + "\") not valid.");
    } else {
        if (options & Directory) {
            while (result[result.length()-1] == '/')
                result = result.substr(0, result.length() - 1);

            if (!S_ISDIR(t.st_mode)) {
                throw Wt::WServer::Exception(varDescription + " (\"" + result
                                             + "\") must be a directory.");
            }
        }

        if (options & RegularFile) {
            if (!S_ISREG(t.st_mode)) {
                throw Wt::WServer::Exception(varDescription + " (\"" + result
                                             + "\") must be a regular file.");
            }
        }
#ifndef WT_WIN32
        if (options & Private) {
            if (t.st_mode & (S_IRWXG | S_IRWXO)) {
                throw Wt::WServer::Exception(varDescription + " (\"" + result
                                             + "\") must be unreadable for group and others.");
            }
        }
#endif
    }
}

} // namespace server
} // namespace Http
