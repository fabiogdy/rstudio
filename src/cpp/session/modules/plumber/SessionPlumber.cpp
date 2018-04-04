/*
 * SessionPlumber.cpp
 *
 * Copyright (C) 2009-18 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

#include "SessionPlumber.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>

#include <core/Algorithm.hpp>
#include <core/Error.hpp>
#include <core/Exec.hpp>
#include <core/FileSerializer.hpp>
#include <core/YamlUtil.hpp>

#include <r/RExec.hpp>
#include <r/RRoutines.hpp>

#include <session/SessionRUtil.hpp>
#include <session/SessionOptions.hpp>
#include <session/SessionModuleContext.hpp>

#define kPlumberTypeFile     "plumber-file"

using namespace rstudio::core;

namespace rstudio {
namespace session {
namespace modules { 
namespace plumber {

namespace {

PlumberFileType getPlumberFileType(const FilePath& filePath, const std::string& contents)
{
   static const boost::regex reRuntimeShiny("runtime:\\s*shiny");

   // Check for 'runtime: shiny' in a YAML header.
   std::string yamlHeader = yaml::extractYamlHeader(contents);
   if (regex_utils::search(yamlHeader.begin(), yamlHeader.end(), reRuntimeShiny))
      return ShinyDocument;

   std::string filename = filePath.filename();

   if (boost::algorithm::iequals(filename, "ui.r") &&
       boost::algorithm::icontains(contents, "shinyUI"))
   {
      return ShinyDirectory;
   }
   else if (boost::algorithm::iequals(filename, "server.r") &&
            boost::algorithm::icontains(contents, "shinyServer"))
   {
      return ShinyDirectory;
   }
   else if (boost::algorithm::iequals(filename, "app.r") &&
            boost::algorithm::icontains(contents, "shinyApp"))
   {
      return ShinyDirectory;
   }
   else if ((boost::algorithm::iequals(filename, "global.r") ||
             boost::algorithm::iequals(filename, "ui.r") ||
             boost::algorithm::iequals(filename, "server.r")) &&
            isShinyAppDir(filePath.parent()))
   {
      return ShinyDirectory;
   }
   else
   {
      // detect standalone single-file Shiny applications
      std::string lastFunction = getLastFunction(contents);
      if (lastFunction == "shinyApp")
         return ShinySingleFile;
      else if (lastFunction == "runApp")
         return ShinySingleExecutable;
   }

   return ShinyNone;
}

std::string onDetectPlumberSourceType(boost::shared_ptr<source_database::SourceDocument> pDoc)
{
   if (!pDoc->path().empty() && pDoc->canContainRCode())
   {
      FilePath filePath = module_context::resolveAliasedPath(pDoc->path());
      PlumberFileType type = getPlumberFileType(filePath, pDoc->contents());
      switch(type)
      {
         default:
         case PlumberFileType::PlumberNone:
            return std::string();
         case PlumberFileType::PlumberFile:
            return kPlumberTypeFile;
      }
   }
   return std::string();
}

Error getPlumberCapabilities(const json::JsonRpcRequest& request, json::JsonRpcResponse* pResponse)
{
   json::Object capsJson;
   capsJson["installed"] = module_context::isPackageInstalled("plumber");
   pResponse->setResult(capsJson);

   return Success();
}

} // anonymous namespace

Error initialize()
{
   using namespace module_context;
   using boost::bind;
   
   events().onDetectSourceExtendedType.connect(onDetectPlumberSourceType);

   return Success();
}

} // namespace plumber
} // namespace modules
} // namespace session
} // namespace rstudio

