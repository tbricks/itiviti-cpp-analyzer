#pragma once

#include "internal/checks/BadRandVisitor.h"
#include "internal/checks/ForRangeConstVisitor.h"
#include "internal/checks/ImproperMoveVisitor.h"
#include "internal/checks/InitMembersVisitor.h"
#include "internal/checks/MiscellaneousVisitor.h"
#include "internal/checks/ReturnValueVisitor.h"

#include "shared/common/UnitedVisitor.h"

namespace ica {

// Both these names must exist;

using ExclusiveTranslationUnitVisitor = UnitedVisitor<
    MiscellaneousVisitor
>;

using ExclusiveTopLevelDeclVisitor = UnitedVisitor<
    BadRandVisitor,
    ForRangeConstVisitor,
    ImproperMoveVisitor,
    InitMembersVisitor,
    ReturnValueVisitor
>;

} // namespace ica
