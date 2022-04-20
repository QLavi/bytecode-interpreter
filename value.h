#pragma once
#include "common.h"

typedef struct Object Object;
typedef struct Object_String Object_String;
typedef enum {
  Vk_Error,
  Vk_Bool,
  Vk_Null,
  Vk_Number,
  Vk_Object,
} Value_Kind;

typedef struct {
  Value_Kind kind;
  union {
    bool boolean;
    double number;
    Object* object;
  };
} value;

#define Value_asBool(val)   (val.boolean)
#define Value_asNumber(val) (val.number)
#define Value_asObject(val) (val.object)

#define Value_Bool(val)   ((value){.kind=Vk_Bool, .boolean=val})
#define Value_Number(val) ((value){.kind=Vk_Number, .number=val})
#define Value_Null()      ((value){.kind=Vk_Null, .number=0})
#define Value_Object(val) ((value){.kind=Vk_Object, .object=(Object*)val})

#define Value_isBool(val)   (val.kind == Vk_Bool)
#define Value_isNumber(val) (val.kind == Vk_Number)
#define Value_isNull(val)   (val.kind == Vk_Null)
#define Value_isObject(val) (val.kind == Vk_Object)
