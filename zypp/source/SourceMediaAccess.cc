/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/source/SourceMediaAccess.cc
 *
*/
#include <iostream>
#include <fstream>

#include "zypp/base/LogTools.h"
#include "zypp/ZYppCallbacks.h"
#include "zypp/source/SourceMediaAccess.h"
//#include "zypp/source/SourceMediaAccessReportReceivers.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
namespace source
{ /////////////////////////////////////////////////////////////////


  SourceMediaAccess::SourceMediaAccess(  const Url &url, const Pathname &path )
      : _url(url),
        _path(path)
  {
    std::vector<media::MediaVerifierRef> single_media;
    single_media[0] = media::MediaVerifierRef(new media::NoVerifier());
    _verifiers = single_media;
  }
  
  SourceMediaAccess::~SourceMediaAccess()
  {
  }

  void SourceMediaAccess::setVerifiers( const std::vector<media::MediaVerifierRef> &verifiers )
  {
    _verifiers = verifiers;
  }

  const Pathname SourceMediaAccess::provideFile(const Pathname & file, const unsigned media_nr )
  {
    callback::SendReport<media::MediaChangeReport> report;
    media::MediaManager media_mgr;
    bool checkonly = false;
    // get the mediaId, but don't try to attach it here
    media::MediaAccessId media = getMediaAccessId( media_nr);
      
    do
    {
      try
      {
        DBG << "Going to try provide file " << file << " from " << media_nr << endl;        
        // try to attach the media
        if ( ! media_mgr.isAttached(media) )
        media_mgr.attach(media);
        media_mgr.provideFile (media, file, false, false);
        break;
      }
      catch ( Exception & excp )
      {
        ZYPP_CAUGHT(excp);
        media::MediaChangeReport::Action user;
        do
        {
          DBG << "Media couldn't provide file " << file << " , releasing." << endl;
          try
          {
            media_mgr.release (media, false);
          }
          catch (const Exception & excpt_r)
          {
              ZYPP_CAUGHT(excpt_r);
              MIL << "Failed to release media " << media << endl;
          }
          
          MIL << "Releasing all medias of all sources" << endl;
          try
          {
            //zypp::SourceManager::sourceManager()->releaseAllSources();
          }
          catch (const zypp::Exception& excpt_r)
          {
              ZYPP_CAUGHT(excpt_r);
              ERR << "Failed to release all sources" << endl;
          }

          // set up the reason
          media::MediaChangeReport::Error reason = media::MediaChangeReport::INVALID;
          
          if( typeid(excp) == typeid( media::MediaFileNotFoundException )  ||
              typeid(excp) == typeid( media::MediaNotAFileException ) )
          {
            reason = media::MediaChangeReport::NOT_FOUND;
          } 
          else if( typeid(excp) == typeid( media::MediaNotDesiredException)  ||
              typeid(excp) == typeid( media::MediaNotAttachedException) )
          {
            reason = media::MediaChangeReport::WRONG;
          }

          user  = checkonly ? media::MediaChangeReport::ABORT :
            report->requestMedia (
              Source_Ref::noSource,
              media_nr,
              reason,
              excp.asUserString()
            );

          DBG << "ProvideFile exception caught, callback answer: " << user << endl;

          if( user == media::MediaChangeReport::ABORT )
          {
            DBG << "Aborting" << endl;
            ZYPP_RETHROW ( excp );
          }
          else if ( user == media::MediaChangeReport::IGNORE )
          {
            DBG << "Skipping" << endl;
            ZYPP_THROW ( SkipRequestedException("User-requested skipping of a file") );
          }
          else if ( user == media::MediaChangeReport::EJECT )
          {
            DBG << "Eject: try to release" << endl;
            try
            {
              //zypp::SourceManager::sourceManager()->releaseAllSources();
            }
            catch (const zypp::Exception& excpt_r)
            {
              ZYPP_CAUGHT(excpt_r);
              ERR << "Failed to release all sources" << endl;
            }
            media_mgr.release (media, true); // one more release needed for eject
            // FIXME: this will not work, probably
          }
          else if ( user == media::MediaChangeReport::RETRY  ||
            user == media::MediaChangeReport::CHANGE_URL )
          {
            // retry
            DBG << "Going to try again" << endl;

            // not attaching, media set will do that for us
            // this could generate uncaught exception (#158620)
            break;
          }
          else
          {
            DBG << "Don't know, let's ABORT" << endl;
            ZYPP_RETHROW ( excp );
          }
        } while( user == media::MediaChangeReport::EJECT );
      }

      // retry or change URL
    } while( true );

    return media_mgr.localPath( media, file );
  }

  media::MediaAccessId SourceMediaAccess::getMediaAccessId (media::MediaNr medianr)
  {
    media::MediaManager media_mgr;

    if (medias.find(medianr) != medias.end())
    {
      media::MediaAccessId id = medias[medianr];
      //if (! noattach && ! media_mgr.isAttached(id))
      //media_mgr.attach(id);
      return id;
    }
    Url url;
    url = rewriteUrl (_url, medianr);
    media::MediaAccessId id = media_mgr.open(url, _path);
    //try {
    //  MIL << "Adding media verifier" << endl;
    //  media_mgr.delVerifier(id);
    //  media_mgr.addVerifier(id, _source.verifier(medianr));
    //}
    //catch (const Exception & excpt_r)
    //{
    //  ZYPP_CAUGHT(excpt_r);
    //  WAR << "Verifier not found" << endl;
    //}
    medias[medianr] = id;
    
    //if (! noattach)
    //  media_mgr.attach(id);

    return id;
  }

  Url SourceMediaAccess::rewriteUrl (const Url & url_r, const media::MediaNr medianr)
  {
    std::string scheme = url_r.getScheme();
    if (scheme == "cd" || scheme == "dvd")
    return url_r;

    DBG << "Rewriting url " << url_r << endl;

    if( scheme == "iso")
    {
      std::string isofile = url_r.getQueryParam("iso");
      boost::regex e("^(.*(cd|dvd))([0-9]+)(\\.iso)$", boost::regex::icase);
      boost::smatch what;
      if(boost::regex_match(isofile, what, e, boost::match_extra))
      {
        Url url( url_r);
        isofile = what[1] + str::numstring(medianr) + what[4];
        url.setQueryParam("iso", isofile);
        DBG << "Url rewrite result: " << url << endl;
        return url;
      }
    }
    else
    {
      std::string pathname = url_r.getPathName();
      boost::regex e("^(.*(cd|dvd))([0-9]+)(/?)$", boost::regex::icase);
      boost::smatch what;
      if(boost::regex_match(pathname, what, e, boost::match_extra))
      {
        Url url( url_r);
        pathname = what[1] + str::numstring(medianr) + what[4];
        url.setPathName(pathname);
        DBG << "Url rewrite result: " << url << endl;
        return url;
      }
    }
    return url_r;
  }

  std::ostream & SourceMediaAccess::dumpOn( std::ostream & str ) const
  {
    return str;
  }

//     media::MediaVerifierRef SourceMediaAccess::verifier(unsigned media_nr)
//     { return media::MediaVerifierRef(new media::NoVerifier()); }

  SourceMediaVerifier::SourceMediaVerifier(const std::string & vendor_r, const std::string & id_r, const media::MediaNr media_nr)
    : _media_vendor(vendor_r)
      , _media_id(id_r)
      , _media_nr(media_nr)
  {}

  bool SourceMediaVerifier::isDesiredMedia(const media::MediaAccessRef &ref)
  {
    if (_media_vendor.empty() || _media_id.empty())
      return true;

      Pathname media_file = "/media." + str::numstring(_media_nr) + "/media";
      ref->provideFile (media_file);
      media_file = ref->localPath(media_file);
      std::ifstream str(media_file.asString().c_str());
      std::string vendor;
      std::string id;

#warning check the stream status
      getline(str, vendor);
      getline(str, id);

      return (vendor == _media_vendor && id == _media_id );
  }


/////////////////////////////////////////////////////////////////
} // namespace source
///////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
