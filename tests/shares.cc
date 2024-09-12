#include "shares.h"

namespace TinyECS_Tests
{

	OrderedFieldIndex<int>			 index1; // D.x
	OrderedFieldIndex<std::string>	 index2; // E.z
	OrderedFieldIndex<int>			 index3; // E.x
	UnorderedFieldIndex<Status>		 index5; // F.status
	UnorderedFieldIndex<bool>		 index6; // G.isX
	UnorderedFieldIndex<std::string> index7; // H.h

} // namespace TinyECS_Tests
