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

#include "placeable.h"

#include "../../graphics/model/models.h"
#include "../../graphics/walkmesh/walkmeshes.h"
#include "../../resource/resources.h"
#include "../../scene/node/model.h"
#include "../../script/types.h"

#include "../game.h"

using namespace std;

using namespace reone::graphics;
using namespace reone::resource;
using namespace reone::scene;
using namespace reone::script;

namespace reone {

namespace game {

Placeable::Placeable(
    uint32_t id,
    Game *game,
    ObjectFactory *objectFactory,
    SceneGraph *sceneGraph
) :
    SpatialObject(id, ObjectType::Placeable, game, objectFactory, sceneGraph) {
}

void Placeable::loadFromGIT(const GffStruct &gffs) {
    string templateResRef(boost::to_lower_copy(gffs.getString("TemplateResRef")));
    loadFromBlueprint(templateResRef);

    loadTransformFromGIT(gffs);
}

void Placeable::loadFromBlueprint(const string &resRef) {
    shared_ptr<GffStruct> utp(_game->services().resource().resources().getGFF(resRef, ResourceType::Utp));
    if (!utp) return;

    loadUTP(*utp);

    shared_ptr<TwoDA> placeables(_game->services().resource().resources().get2DA("placeables"));
    string modelName(boost::to_lower_copy(placeables->getString(_appearance, "modelname")));

    auto model = make_shared<ModelSceneNode>(_game->services().graphics().models().get(modelName), ModelUsage::Placeable, _sceneGraph);
    model->setCullable(true);
    model->setDrawDistance(64.0f);
    _sceneNode = move(model);

    _walkmesh = _game->services().graphics().walkmeshes().get(modelName, ResourceType::Pwk);
}

void Placeable::loadTransformFromGIT(const GffStruct &gffs) {
    _position[0] = gffs.getFloat("X");
    _position[1] = gffs.getFloat("Y");
    _position[2] = gffs.getFloat("Z");

    _orientation = glm::quat(glm::vec3(0.0f, 0.0f, gffs.getFloat("Bearing")));

    updateTransform();
}

bool Placeable::isSelectable() const {
    return _usable;
}

shared_ptr<Walkmesh> Placeable::getWalkmesh() const {
    return _walkmesh;
}

void Placeable::runOnUsed(shared_ptr<SpatialObject> usedBy) {
    if (!_onUsed.empty()) {
        _game->services().scriptRunner().run(_onUsed, _id, usedBy ? usedBy->id() : kObjectInvalid);
    }
}

void Placeable::runOnInvDisturbed(shared_ptr<SpatialObject> triggerrer) {
    if (!_onInvDisturbed.empty()) {
        _game->services().scriptRunner().run(_onInvDisturbed, _id, triggerrer ? triggerrer->id() : kObjectInvalid);
    }
}

} // namespace game

} // namespace reone
