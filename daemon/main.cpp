#include <config.hpp>  // for ensure_config
#include <fs.hpp>
#include <getopt.h>
#include <libgen.h>
#include <llarp.h>
#include <logger.hpp>
#include <signal.h>

#include <string>
#include <iostream>

#ifdef _WIN32
#define wmin(x, y) (((x) < (y)) ? (x) : (y))
#define MIN wmin
#endif

struct llarp_main *ctx = 0;

void
handle_signal(int sig)
{
  if(ctx)
    llarp_main_signal(ctx, sig);
}

int
printHelp(const char *argv0, int code = 1)
{
  std::cout << "usage: " << argv0 << " [-h] [-v] [-g|-c] config.ini"
            << std::endl;
  return code;
}

#ifdef _WIN32
int
startWinsock()
{
  WSADATA wsockd;
  int err;
  err = ::WSAStartup(MAKEWORD(2, 2), &wsockd);
  if(err)
  {
    perror("Failed to start Windows Sockets");
    return err;
  }
  return 0;
}
#endif

int
main(int argc, char *argv[])
{
  bool multiThreaded          = true;
  const char *singleThreadVar = getenv("LLARP_SHADOW");
  if(singleThreadVar && std::string(singleThreadVar) == "1")
  {
    multiThreaded = false;
  }

#ifdef _WIN32
  if(startWinsock())
    return -1;
#endif

  int opt            = 0;
  bool genconfigOnly = false;
  bool asRouter      = false;
  bool overWrite     = false;
  while((opt = getopt(argc, argv, "hgcfrv")) != -1)
  {
    switch(opt)
    {
      case 'v':
        SetLogLevel(llarp::eLogDebug);
        llarp::LogDebug("debug logging activated");
        break;
      case 'h':
        return printHelp(argv[0], 0);
      case 'g':
        genconfigOnly = true;
        break;
      case 'c':
        genconfigOnly = true;
        break;
      case 'r':
#ifdef _WIN32
        llarp::LogWarn(
            "please don't try this at home, the relay feature is untested on "
            "windows server --R.");
#endif
        asRouter = true;
        break;
      case 'f':
        overWrite = true;
        break;
      default:
        return printHelp(argv[0]);
    }
  }

  std::string conffname;

  if(optind < argc)
  {
    // when we have an explicit filepath
    fs::path fname   = fs::path(argv[optind]);
    fs::path basedir = fname.parent_path();
    conffname        = fname.string();
    if(basedir.string().empty())
    {
      if(!llarp_ensure_config(fname.string().c_str(), nullptr, overWrite,
                              asRouter))
        return 1;
    }
    else
    {
      std::error_code ec;
      if(!fs::create_directories(basedir, ec))
      {
        if(ec)
        {
          llarp::LogError("failed to create '", basedir.string(),
                          "': ", ec.message());
          return 1;
        }
      }
      if(!llarp_ensure_config(fname.string().c_str(), basedir.string().c_str(),
                              overWrite, asRouter))
        return 1;
    }
  }
  else
  {
// no explicit config file provided
#ifdef _WIN32
    fs::path homedir = fs::path(getenv("APPDATA"));
#else
    fs::path homedir = fs::path(getenv("HOME"));
#endif
    fs::path basepath = homedir / fs::path(".lokinet");
    fs::path fpath    = basepath / "lokinet.ini";

    std::error_code ec;
    // These paths are guaranteed to exist - $APPDATA or $HOME
    // so only create .lokinet/*
    if(!fs::create_directory(basepath, ec))
    {
      if(ec)
      {
        llarp::LogError("failed to create '", basepath.string(),
                        "': ", ec.message());
        return 1;
      }
    }

    if(!llarp_ensure_config(fpath.string().c_str(), basepath.string().c_str(),
                            overWrite, asRouter))
      return 1;
    conffname = fpath.string();
  }

  if(genconfigOnly)
    return 0;

  ctx      = llarp_main_init(conffname.c_str(), multiThreaded);
  int code = 1;
  if(ctx)
  {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#ifndef _WIN32
    signal(SIGHUP, handle_signal);
#endif
    code = llarp_main_setup(ctx);
    if(code == 0)
      code = llarp_main_run(ctx);
    llarp_main_free(ctx);
  }
#ifdef _WIN32
  ::WSACleanup();
#endif
  exit(code);
  return code;
}
