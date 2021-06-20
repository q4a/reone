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

#include "cursors.h"

#include "../common/streamutil.h"
#include "../graphics/cursor.h"
#include "../graphics/services.h"
#include "../graphics/texture/curreader.h"
#include "../resource/resources.h"
#include "../resource/services.h"

using namespace std;

using namespace reone::graphics;
using namespace reone::resource;

namespace reone {

namespace game {

static unordered_map<CursorType, pair<uint32_t, uint32_t>> g_namesByResRefKotorTsl = {
    { CursorType::Default, { 4, 5 } },
    { CursorType::Talk, { 14, 15 } },
    { CursorType::Door, { 26, 27 } },
    { CursorType::Pickup, { 28, 29 } },
    { CursorType::DisableMine, { 36, 37 } },
    { CursorType::RecoverMine, { 40, 41 } },
    { CursorType::Attack, { 54, 55 } }
};

static unordered_map<CursorType, pair<uint32_t, uint32_t>> g_namesByResRefSteamTsl = {
    { CursorType::Default, { 3, 4 } },
    { CursorType::Talk, { 13, 14 } },
    { CursorType::Door, { 25, 26 } },
    { CursorType::Pickup, { 27, 28 } },
    { CursorType::DisableMine, { 35, 36 } },
    { CursorType::RecoverMine, { 39, 40 } },
    { CursorType::Attack, { 53, 54 } }
};

Cursors::Cursors(GameID gameId, GraphicsServices &graphics, ResourceServices &resource) :
    _gameId(gameId),
    _graphics(graphics),
    _resource(resource) {
}

Cursors::~Cursors() {
    deinit();
}

void Cursors::deinit() {
    _cache.clear();
}

shared_ptr<Cursor> Cursors::get(CursorType type) {
    auto maybeCursor = _cache.find(type);
    if (maybeCursor != _cache.end()) {
        return maybeCursor->second;
    }
    const pair<uint32_t, uint32_t> &names = getCursorNames(type);
    shared_ptr<Texture> up(newTexture(names.first));
    shared_ptr<Texture> down(newTexture(names.second));

    auto cursor = make_shared<Cursor>(up, down, _graphics);
    auto inserted = _cache.insert(make_pair(type, cursor));

    return inserted.first->second;
}

const pair<uint32_t, uint32_t> &Cursors::getCursorNames(CursorType type) {
    auto &nameByResRef = (_gameId == GameID::TSL_Steam) ? g_namesByResRefSteamTsl : g_namesByResRefKotorTsl;
    return getCursorNames(type, nameByResRef);
}

const pair<uint32_t, uint32_t> &Cursors::getCursorNames(CursorType type, const unordered_map<CursorType, pair<uint32_t, uint32_t>> &nameByResRef) {
    auto maybeName = nameByResRef.find(type);
    if (maybeName == nameByResRef.end()) {
        throw logic_error("Cursor names not found by type " + to_string(static_cast<int>(type)));
    }
    return maybeName->second;
}

shared_ptr<Texture> Cursors::newTexture(uint32_t name) {
    shared_ptr<ByteArray> data(_resource.resources().getFromExe(name, PEResourceType::Cursor));

    CurReader curFile;
    curFile.load(wrap(data));

    return curFile.texture();
}

} // namespace game

} // namespace reone
