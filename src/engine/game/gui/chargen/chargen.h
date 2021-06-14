/*
 * Copyright (c) 2020-2021 The reone project contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "../../../scene/node/model.h"
#include "../../../scene/scenegraph.h"

#include "../../character.h"

#include "../gui.h"

#include "abilities.h"
#include "classselect.h"
#include "custom.h"
#include "feats.h"
#include "levelup.h"
#include "nameentry.h"
#include "portraitselect.h"
#include "skills.h"
#include "quick.h"
#include "quickorcustom.h"

namespace reone {

namespace game {

enum class CharGenScreen {
    ClassSelection,
    QuickOrCustom,
    Quick,
    PortraitSelection,
    Name,
    Custom,
    Abilities,
    Skills,
    Feats,
    LevelUp
};

class CharacterGeneration : public GameGUI {
public:
    enum class Type {
        Quick,
        Custom,
        LevelUp
    };

    CharacterGeneration(Game *game);

    void load() override;
    bool handle(const SDL_Event &event) override;
    void update(float dt) override;
    void draw() override;
    void draw3D() override;

    void startQuick();
    void startCustom();
    void startLevelUp();

    void finish();
    void cancel();

    void openClassSelection();
    void openNameEntry();
    void openAbilities();
    void openSkills();
    void openFeats();
    void openPortraitSelection();
    void openQuick();
    void openQuickOrCustom();
    void openCustom();
    void openSteps();
    void openLevelUp();

    void goToNextStep();

    const Character &character() const { return _character; }

    void setCharacter(Character character);

private:
    CharGenScreen _screen { CharGenScreen::ClassSelection };
    Type _type  { Type::Quick };
    Character _character;

    // Sub GUI

    std::unique_ptr<ClassSelection> _classSelection;
    std::unique_ptr<QuickOrCustom> _quickOrCustom;
    std::unique_ptr<QuickCharacterGeneration> _quick;
    std::unique_ptr<CustomCharacterGeneration> _custom;
    std::unique_ptr<PortraitSelection> _portraitSelection;
    std::unique_ptr<CharGenAbilities> _abilities;
    std::unique_ptr<CharGenSkills> _skills;
    std::unique_ptr<CharGenFeats> _feats;
    std::unique_ptr<NameEntry> _nameEntry;
    std::unique_ptr<LevelUpMenu> _levelUp;

    // END Sub GUI

    void reloadCharacterModel();
    void updateAttributes();
    void changeScreen(CharGenScreen screen);

    gui::GUI *getSubGUI() const;
    std::shared_ptr<scene::ModelSceneNode> getCharacterModel(scene::SceneGraph &sceneGraph);

    void setAttributesVisible(bool visible);

    // Loading

    void loadClassSelection();
    void loadQuickOrCustom();
    void loadQuick();
    void loadPortraitSelection();
    void loadNameEntry();
    void loadCustom();
    void loadAbilities();
    void loadSkills();
    void loadFeats();
    void loadLevelUp();

    // END Loading
};

} // namespace game

} // namespace reone
