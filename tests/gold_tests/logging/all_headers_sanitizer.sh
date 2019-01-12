#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

# Spaz that it is, Trafficserver will pseudo-randomly change the order of headers. So it's necessary to get the header / value
# pairs on different lines and then do a sort, before comparing to a gold file.

sed 's/{{\([^{]\)/\
{{\1/g' | \
sed 's/\([^}]\)}}/\1}}\
/g' | \
sed 's/ {/\
{/g' | sort
