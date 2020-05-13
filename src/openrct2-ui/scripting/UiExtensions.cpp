/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifdef ENABLE_SCRIPTING

#    include "UiExtensions.h"

#    include "CustomMenu.h"
#    include "ScTileSelection.hpp"
#    include "ScUi.hpp"
#    include "ScWidget.hpp"
#    include "ScWindow.hpp"

#    include <openrct2/scripting/ScriptEngine.h>

using namespace OpenRCT2::Scripting;

void UiScriptExtensions::Extend(ScriptEngine& scriptEngine)
{
    auto ctx = scriptEngine.GetContext();

    dukglue_register_global(ctx, std::make_shared<ScUi>(scriptEngine), "ui");

    ScTileSelection::Register(ctx);
    ScTool::Register(ctx);
    ScUi::Register(ctx);
    ScViewport::Register(ctx);
    ScWidget::Register(ctx);
    ScCheckBoxWidget::Register(ctx);
    ScDropdownWidget::Register(ctx);
    ScListViewWidget::Register(ctx);
    ScWindow::Register(ctx);

    InitialiseCustomMenuItems(scriptEngine);
}

#endif
