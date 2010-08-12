#include "cartographer/config.h" /* Обязательно первым */

#include <mylib.h>

/* std */
#include <cmath>
#include <cstddef> /* std::size_t */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <istream>
#include <iterator>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/* boost */
#include <boost/algorithm/string.hpp>
#include <boost/archive/detail/utf8_codecvt_facet.hpp>
#include <boost/bind.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/special_defs.hpp>
#include <boost/date_time/time_parsing.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/functional/hash.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/io/ios_state.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/ptr_container/ptr_list.hpp>
#include <boost/regex.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/system/system_error.hpp>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <boost/utility.hpp>

/* wx */
#ifdef WX_PRECOMP

#include <wx/dcgraph.h> /* wxGCDC и wxGraphicsContext */
#include <wx/mstream.h>  /* wxMemoryInputStream */
#include <wx/glcanvas.h> /* OpenGL */

#endif
