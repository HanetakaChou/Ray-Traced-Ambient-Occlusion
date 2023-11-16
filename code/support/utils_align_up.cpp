//
// Copyright (C) YuqiaoZhang(HanetakaChou)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "utils_align_up.h"
#include <assert.h>

uint32_t utils_align_up(uint32_t value, uint32_t alignment)
{
	//
	//  Copyright (c) 2005-2019 Intel Corporation
	//
	//  Licensed under the Apache License, Version 2.0 (the "License");
	//  you may not use this file except in compliance with the License.
	//  You may obtain a copy of the License at
	//
	//      http://www.apache.org/licenses/LICENSE-2.0
	//
	//  Unless required by applicable law or agreed to in writing, software
	//  distributed under the License is distributed on an "AS IS" BASIS,
	//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	//  See the License for the specific language governing permissions and
	//  limitations under the License.
	//

	// [alignUp](https://github.com/oneapi-src/oneTBB/blob/tbb_2019/src/tbbmalloc/shared_utils.h#L42)

	assert(alignment != static_cast<uint32_t>(0));

	// power-of-2 alignment
	assert((alignment & (alignment - static_cast<uint32_t>(1))) == static_cast<uint32_t>(0));

	return (((value - static_cast<uint32_t>(1)) | (alignment - static_cast<uint32_t>(1))) + static_cast<uint32_t>(1));
}

uint64_t utils_align_up(uint64_t value, uint64_t alignment)
{
	//
	//  Copyright (c) 2005-2019 Intel Corporation
	//
	//  Licensed under the Apache License, Version 2.0 (the "License");
	//  you may not use this file except in compliance with the License.
	//  You may obtain a copy of the License at
	//
	//      http://www.apache.org/licenses/LICENSE-2.0
	//
	//  Unless required by applicable law or agreed to in writing, software
	//  distributed under the License is distributed on an "AS IS" BASIS,
	//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	//  See the License for the specific language governing permissions and
	//  limitations under the License.
	//

	// [alignUp](https://github.com/oneapi-src/oneTBB/blob/tbb_2019/src/tbbmalloc/shared_utils.h#L42)

	assert(alignment != static_cast<uint64_t>(0));

	// power-of-2 alignment
	assert((alignment & (alignment - static_cast<uint64_t>(1))) == static_cast<uint64_t>(0));

	return (((value - static_cast<uint64_t>(1)) | (alignment - static_cast<uint64_t>(1))) + static_cast<uint64_t>(1));
}
