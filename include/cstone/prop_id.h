#ifndef PROP_ID_H
#define PROP_ID_H

/* SPDX-License-Identifier: MIT
Copyright 2021 Kevin Thibedeau
(kevin 'period' thibedeau 'at' gmail 'punto' com)

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/
/*
------------------------------------------------------------------------------
This details a system for encoding a hierarchical schema into numeric property IDs. These
constructed IDs can then be used for keyed lookup of property values, to generate names
as readable strings, or sent via binary protocols. This affords a more compact
representation of property IDs than using string keys directly.

This system only pertains to encoding property IDs. It does not specify property values,
read-only vs read-write, persistence, or any other implementation details.

These IDs can also be used as unique identifiers without an associated value for things
like events, errors, or object IDs.


Encoding
--------

Properties are represented with a uint32_t split into four 8-bit fields P1-P4 ranging from
MSB to LSB. The fields have values that place a property within a hierarchical tree.

  [        Property ID         ]
  [ P1 ]  [ P2 ]  [ P3 ]  [ P4 ]
  31- 24  23- 16  15 - 8  7 -  0

The P1 field is at the top of the heirarchy descending down to P4.

Each field has a numeric value with an associated name generated by an expansion of the
PROP_LIST() X-macro. Fields are given symbolic names of the form "P<1-4>_<name>". A
property is constructed by bitwise ORing the field names that compose the property:

  #define P_SUBNET_MASK (P1_NET | P2_IPV4 | P3_SUBNET | P4_MASK)

A property can be represented with its named fields in dotted format:

  "NET.IPV4.SUBNET.MASK" or "net.ipv4.subnet.mask"

It can also be represented with its numerical value in hex after a leading 'P' char:

  "P01020304"


Schemata
--------

There is no fixed schema for organizing the property names. However, a suggested general
format is to identify an "entity" with P1-P3 and use P4 for any attributes of the entity.
If an entity doesn't need named attributes then it should default to using P4_VALUE for
the last field. If a value field may have multiple interpretations, use the P4_KIND
attribute to disambiguate them.

Example schema:

  P1:  System
  P2:    Subsystem
  P3:      Entity
  P4:        Attribute

If three levels of hierarchy are insufficient then P4 can be used as an entity identifier:

  P1:  System
  P2:    Subsystem
  P3:      Component
  P4:        Entity


When properties are used as keys referencing an associated value that value can be of any
type. You will either need to establish a convention for the type of a property, use the
P4_KIND attribute, or use another mechamism to determine a value type.


Array fields
------------

Property fields P1-P3 can be marked as an "array" by setting their upper bit. The following
field is then interpreted as an array index. Indices can range from 0 to 254. 

The Px_ARR() macros are used to mark a field as an array and set the index in the next field:

  // Property SYS.HW[10].VALUE
  #define P_FOO  (P1_SYS | P2_HW | P2_ARR(10) | P4_VALUE)

The field following the array will be omitted (P3 here) since it is used as the index.

Normally you'd construct a property macro with the index set to 0. Then you can use the
PROP_SET_INDEX() macro to generate a specific indexed property dynamically.

Arrays reduce the number of levels in the property hierarchy. The P4 field can't be marked
as an array since there is no P5 to serve as an index. Leaf attributes are not available when
P3 is an array.

The following array formats can be used:

  P1[n].P3.P4
  P1.P2[n].P4
  P1.P2.P3[n]
  P1[n].P3[n]

P1+P2 and P2+P3 cannot be marked as arrays in the same property since that would overlap fields
with array indices.

If an array field has a variable number of items then there should be another property
with the same field name but without the array bit set and "COUNT" as the P4 field. Any
intermediate fields will use "INFO".

  Array property:   Number of items:
  P1.P2.P3[n]       P1.P2.P3.COUNT
  P1.P2[n].P4       P1.P2.INFO.COUNT
  P1[n].P3.P4       P1.INFO.INFO.COUNT
  P1[n].P3[n]       P1.INFO.INFO.COUNT  P1[n].P3.COUNT


Masks
-----

The field value 255 (0xFF) serves as a mask indicator. Property queries can be constructed
with a field set to 255 indicating that the field is not considered when doing matching
comparisons against properties. A masked field will match any value in a property field.

Consider the property:        P0101FF01
The mask is:                  M0000FF00

The property matches any of:
                              P01010201
                              P0101AA01
                              P0101BB01
But not:
                              P0102AA01


Reserved fields
---------------

Because the upper bit is reserved for arrays, property fields can range from 0 to 127. Field
values 0, 127 (0x7F), and 255 (0xFF) are reserved. For the P1 field, the values 120-126
(0x78-0x7E) are also reserved. The upper bit will never be set for P4 unless it's an index
for P3.

255 is reserved for masks in all fields. An all 0's property P00000000 will never be valid.
It represents "no property".

Standardized fields will generally be assigned to the range 1 to 63. Application specific
fields should have values in the range 64 to 126.


Naming
------

Properties assembled from field macros can be given any name but it best to stick to a
consistent naming scheme so that users can understand what the fields mean and which are used
for arrays. The examples above use short names for brevity but the following convention is
recommended:

  * Names are prefixed with "P_"
  * Field names are concatenated with '_' excluding any field prefix ("P1_", "P2_", etc.)
  * Array index fields are represented with a lowercase "n". All other text is uppercase.

  #define P_SYS_HW_n_VALUE  (P1_SYS | P2_HW | P2_ARR(0) | P4_VALUE)

The human readable representation will then be the comparable "SYS.HW[0].VALUE"

Identical field names can be used in multiple places within a property. They are distinguished
in the macro defintions by their different "Px_" prefixes. Ideally they should all have the same
numeric value for the sake of consistency but that isn't required.

The idea behind the property scheme is that a full property name can be constructed from short
reusable components. This minimizes the number of literal strings that need to be stored in a
compiled binary if you need to convert numeric properties into a readable string. Consider you
have a system with 100 properties averaging 32 bytes long. You'd need 3.2KB for static string
storage. If you compose those 100 properties from 40 component strings averaging 8 bytes each,
you'd only need 320 bytes of static storage.

Field names should be kept uppercase to maintain all-caps macros. The exception for this is if
a name is concatenated from multiple words. The '_' char is already used to separate fields
and should be avoided within field names. For this situation, fields can be named with
CamelCase to make them more readable. :c:func:`prop_parse_name` is unaffected as it is case
insensitive.

  Ex: P1_APP | P2_GUI | P3_MESSAGE | P4_MultiWordField
      P_APP_GUI_MESSAGE_MultiWordField
      app.gui.message.MultiWordField

Namespaces
----------

Property definitions are grouped into namespace structs that manage data used to convert between
numeric field values and human readable property names. You can have multiple independent
namespaces associated with subsystems so that their properties don't have to be centrally
defined. Each namespace has a prefix value and mask that determine which properties are covered
by the namespace. There is a default global namespace with common field definitions with a
prefix of 0. You can add one additional application specific namespace with a 0 prefix. Any
other namespaces must have a unique prefix value that covers a subset of the property encodings.
It is possible to use the same field values within two different namespaces though it is
recommended to avoid doing this to prevent confusion about a property name. Namespace handling
does not affect the numerical representation of a property. They remain globally unique.

You create a namespace by preparing a :c:struct:`PropNamespace` object with an array of
:c:struct:`PropFieldDef` assigned to its ``prop_defs`` member. After initializing the property
system with :c:func:`prop_init` you can add a new namespace with :c:func:`prop_add_namespace`.


Booleans
--------

Boolean settings can create a profusion of properties. This may or may not be an issue in an
application. It is recommended to condense booleans under a shared branch of the hierarchy into
a single property with the P4_FLAGS attribute. Then you can use individual bits in the property
value to store boolean settings.


Events
------

Events can be regarded as a kind of ephemeral property. As such you may wish to use the
property encoding scheme to describe events as well. If you don't need an event system to
interoperate with properties, it is safe to use the entire number space for their encoding. If
you want to have APIs that can process properties and events together then the P1_EVENT field
should be used to ensure events never collide with a property.

  // Example event EVENT.BUTTON[3].PRESS
  #define P_EVENT_BUTTON_n_PRESS  (P1_EVENT | P2_BUTTON | P2_ARR(0) | P4_PRESS)
  uint32_t event = PROP_SET_INDEX(P_EVENT_BUTTON_n_PRESS, 2, 3);


Error codes
-----------

The property encoding can also be used for error codes. To ease the management of defining
errors it is suggested that you use the P1.P2.P3[n] format to identify the entity associated
with the error and have an enumerated type for the array index that encodes the specific
error.

You can have the error codes in their own namespace or use the P1_ERROR field to coexist
with global properties.

  #define P_ERROR_HTTP_REMOTE_n   (P1_ERROR | P2_HTTP | P3_REMOTE | P3_ARR(0))


Resources
---------

The umsg message passing interface uses prop IDs to identify a target that a subscriber
can listen to. The P1_RSRC field creates a dedicated namespace for targets and other
internal system objects in need of an ID that should be distinct from other properties.
------------------------------------------------------------------------------
*/

// ******************** Configuration ********************
// FIXME: Move to common cstone config header
#define USE_PROP_ID_FIELD_NAMES
#define USE_PROP_ID_REV_HASH

#if defined USE_PROP_ID_REV_HASH && !defined USE_PROP_ID_FIELD_NAMES
#  error "USE_PROP_ID_REV_HASH requires USE_PROP_ID_FIELD_NAMES"
#endif


#ifdef USE_PROP_ID_REV_HASH
#  include "util/dhash.h"
#endif

// ******************** Field operations ********************

// Field shift values
#define SP1 24
#define SP2 16
#define SP3 8
#define SP4 0

// Mask for a single field level
#define PROP_MASK(level)        ((uint32_t)(0xFFul << ((4-(level))*8)))
#define PROP_ARR_MASK(level)    ((uint32_t)(0x7Ful << ((4-(level))*8)))
#define PROP_PREFIX_MASK(level) ((uint32_t)((0xFFFFFFFFul << ((4-(level))*8)) & 0xFFFFFFFFul))

// Extract mask from a composed property
#define PROP_GET_MASK(p)  (~((((p) & P1_MSK) == P1_MSK ? P1_MSK : 0) | \
                          (((p) & P2_MSK) == P2_MSK ? P2_MSK : 0) | \
                          (((p) & P3_MSK) == P3_MSK ? P3_MSK : 0) | \
                          (((p) & P4_MSK) == P4_MSK ? P4_MSK : 0)))

#define PROP_FIELD(p, level) (((p) & PROP_MASK(level)) >> ((4-(level))*8))

//#define PROP_PREFIX(p, level) ((p) & PROP_PREFIX_MASK(level))

// Define array fields
#define P1_ARR(index)  ((uint32_t)(0x80ul << SP1) | ((index) << SP2))
#define P2_ARR(index)  ((uint32_t)(0x80ul << SP2) | ((index) << SP3))
#define P3_ARR(index)  ((uint32_t)(0x80ul << SP3) | ((index) << SP4))

// Test for presence of an array field
#define PROP_HAS_ARRAY(p)       (((p) & 0x80808000ul) != 0)
#define PROP_FIELD_IS_ARRAY(p)  (((p) & 0x80ul) != 0)

// Strip upper bit from array fields
#define PROP_FROM_ARRAY(p) ((uint32_t)((p) & ~0x80808000ul))


#define PROP_SET_INDEX(p, level, index)  \
    (((p) & ~PROP_MASK((level)+1)) | (((index) & 0xFF) << ((3-(level))*8)))
#define PROP_GET_INDEX(p, level)  ((p & PROP_MASK((level)+1)) >> ((3-(level))*8))


/*
sys.info.clock.freq

app.info.build.version
app.info.build.timestamp

sensor.limit.temp.max
sensor.limit.volt.max
sensor.limit.volt.min
sensor.info.temp.value

sensor.temp.limit.max
sensor.temp.limit.min
sensor.temp[0].value
sensor.volt.limit.max

hw.outlet[n].name
hw.lcd.brightness.value
hw.touch.cal[n]

net.icmp.target.name
net.svc1.remote.url
net.svc1.proxy.url
net.http.local.port
net.http.secure.port
net.ipv4.subnet.mask
net.ipv4.address.value
net.ipv4.gateway.value
net.ipv4.dns[n]
net.ipv4.dns.count
net.mac.address.value
net.info.local.name
net.info.domain.name

net.stats.rx.count
net.stats.tx.count

app.gui.theme[n]


rsrc.gui.local.widget
rsrc.ssh.target[0]      // Unique IDs for umsg targets
cmd.http.local[3]       // Imperative command to an entity
event.button[2].press   // Events
event.touch.info.press
error.ipv4.dns[0]       // Error codes

*/

// ******************** Standard fields ********************

// STM32F1 headers have a "USB" macro that conflicts here
#ifdef USB
#  undef USB
#endif

#define PROP_LIST(M) \
M(P1, APP,      1) \
M(P1, SYS,      2) \
M(P1, HW,       3) \
M(P1, STATS,    4) \
M(P1, NET,      5) \
M(P1, SENSOR,   6) \
M(P1, RSRC,     7) \
M(P1, CMD,      8) \
M(P1, EVENT,    9) \
M(P1, WARN,     10) \
M(P1, AUX_8_16, 11) \
M(P1, AUX_24,   12) \
M(P1, DEBUG,    13) \
M(P1, ERROR,    14) \
M(P1, R120,     120) \
M(P1, R121,     121) \
M(P1, R122,     122) \
M(P1, R123,     123) \
M(P1, R124,     124) \
M(P1, R125,     125) \
M(P1, R126,     126) \
M(P1, R127,     127) \
\
M(P2, INFO,     1) \
M(P2, SYS,      2) \
M(P2, HW,       3) \
M(P2, STORAGE,  4) \
M(P2, CON,      5) \
M(P2, USB,      6) \
M(P2, SPI,      7) \
M(P2, I2C,      8) \
M(P2, CRON,     9) \
M(P2, PRNG,     10) \
M(P2, BUTTON,   11) \
M(P2, R127,     127) \
\
M(P3, INFO,     1) \
M(P3, LOCAL,    2) \
M(P3, REMOTE,   3) \
M(P3, MESSAGE,  4) \
M(P3, PROP,     5) \
M(P3, TARGET,   6) \
M(P3, LIMIT,    7) \
M(P3, BUILD,    8) \
M(P3, CRON,     9) \
M(P3, MEM,      10) \
M(P3, R127,     127) \
\
M(P4, VALUE,    1) \
M(P4, KIND,     2) \
M(P4, NAME,     3) \
M(P4, COUNT,    4) \
M(P4, VERSION,  5) \
M(P4, MIN,      6) \
M(P4, MAX,      7) \
M(P4, FLAGS,    8) \
M(P4, TIMEOUT,  9) \
M(P4, INVALID,  10) \
M(P4, ACCESS,   11) \
M(P4, UPDATE,   12) \
M(P4, TASK,     13) \
M(P4, QUERY,    14) \
M(P4, SUSPEND,  15) \
M(P4, RESUME,   16) \
M(P4, ATTACH,   17) \
M(P4, DETACH,   18) \
M(P4, SIZE,     19) \
M(P4, LOC,      20) \
M(P4, PRESS,    21) \
M(P4, RELEASE,  22) \
M(P4, ON,       23) \
M(P4, OFF,      24) \
M(P4, R127,     127) \
\
M(P1, MSK, 0xFFul) \
M(P2, MSK, 0xFFul) \
M(P3, MSK, 0xFFul) \
M(P4, MSK, 0xFFul)


#define PNAME(level, name)  level##_##name

// Expand X-macro into enum for each standard field
enum PropElements {
#define PROP_ENUM_ITEM(level, name, val)  PNAME(level,name) = (val << S##level),
  PROP_LIST(PROP_ENUM_ITEM)
};


// Manipulate auxilliary prop encodings
#define PROP_AUX_8_16(n8, n16)  (P1_AUX_8_16 | (((n8) & 0xFFul) << 16) | ((n16) & 0xFFFFul))
#define PROP_AUX_24(n)          (P1_AUX_24 | ((n) & 0xFFFFFFul))

#define PROP_AUX_8_VALUE(n)     (((n) >> 16) & 0xFFul)
#define PROP_AUX_16_VALUE(n)    ((n) & 0xFFFFul)
#define PROP_AUX_24_VALUE(n)    ((n) & 0xFFFFFFul)

#define PROP_AUX_8_16_MASK      (P1_AUX_8_16 | P2_MSK | P3_MSK | P4_MSK)
#define PROP_AUX_24_MASK        (P1_AUX_24 | P2_MSK | P3_MSK | P4_MSK)


// Format string for prop IDs
#define PROP_ID   "P%08" PRIX32

typedef struct {
  uint32_t field;
  const char *name;
} PropFieldDef;

typedef struct PropNamespace PropNamespace;

struct PropNamespace {
  struct PropNamespace *next;
  uint32_t prefix;
  uint32_t mask;
  PropFieldDef *prop_defs;  // Sorted array for name lookup from field value
  size_t prop_defs_len;

#ifdef USE_PROP_ID_REV_HASH
  dhash name_index;   // Table for field value lookup from name
#endif
};

// Generate definitions for namespace hash table
#define PROP_FIELD_DEF(level, ename, val)   {.field = PNAME(level, ename), .name = #level #ename},

#define P_SYS_PRNG_LOCAL_VALUE        (P1_SYS | P2_PRNG | P3_LOCAL | P4_VALUE)
#define P_SYS_STORAGE_INFO_COUNT      (P1_SYS | P2_STORAGE | P3_INFO | P4_COUNT)
#define P_SYS_CRON_LOCAL_VALUE        (P1_SYS | P2_CRON | P3_LOCAL | P4_VALUE)
#define P_ERROR_SYS_MEM_ACCESS        (P1_ERROR | P2_SYS | P3_MEM | P4_ACCESS)

#ifdef __cplusplus
extern "C" {
#endif

void prop_init(void);
void prop_add_namespace(PropNamespace *ns);

char *prop_get_name(uint32_t prop, char *buf, size_t buf_size);
uint32_t prop_parse_id(const char *id);
uint32_t prop_parse_name(const char *name);
uint32_t prop_parse_any(const char *id_name);
bool prop_is_valid(uint32_t prop, bool allow_mask);
bool prop_has_mask(uint32_t prop);
bool prop_match(uint32_t prop, uint32_t masked_prop);

uint32_t prop_new_global_id(void);

#ifdef __cplusplus
}
#endif

#endif // PROP_ID_H
