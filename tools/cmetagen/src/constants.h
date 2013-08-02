#ifndef __CONSTANTS_H
#define __CONSTANTS_H

#include <string>

namespace metagen {

const std::string includeMetaDefine("cpgf/gmetadefine.h");
const std::string includeMetaDataHeader("cpgf/metadata/private/gmetadata_header.h");
const std::string includeMetaDataFooter("cpgf/metadata/private/gmetadata_footer.h");
const std::string includeMetaPolicy("cpgf/gmetapolicy.h");
const std::string includeScriptBindUtil("cpgf/scriptbind/gscriptbindutil.h");
const std::string includeScriptWrapper("cpgf/scriptbind/gscriptwrapper.h");
const std::string includeScopedInterface("cpgf/gscopedinterface.h");

const std::string metaTypeTypeDefName("MetaType");

} // namespace metagen


#endif