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

#include "emitternode.h"

#include <stdexcept>

#include "glm/gtc/constants.hpp"

#include "../../common/random.h"

#include "modelscenenode.h"
#include "particlenode.h"

using namespace std;

using namespace reone::render;

namespace reone {

namespace scene {

static constexpr int kMaxParticleCount = 24;

EmitterSceneNode::EmitterSceneNode(const ModelSceneNode *modelSceneNode, const shared_ptr<Emitter> &emitter, SceneGraph *sceneGraph) :
    SceneNode(sceneGraph),
    _modelSceneNode(modelSceneNode),
    _emitter(emitter) {

    if (!modelSceneNode) {
        throw invalid_argument("modelSceneNode must not be null");
    }
    if (!emitter) {
        throw invalid_argument("emitter must not be null");
    }

    init();
}

void EmitterSceneNode::init() {
    if (_emitter->birthrate() != 0) {
        _birthInterval = 1.0f / static_cast<float>(_emitter->birthrate());
    }
}

void EmitterSceneNode::update(float dt) {
    removeExpiredParticles(dt);
    spawnParticles(dt);
    SceneNode::update(dt);
}

void EmitterSceneNode::spawnParticles(float dt) {
    switch (_emitter->updateType()) {
        case Emitter::UpdateType::Fountain:
            if (_emitter->birthrate() != 0.0f) {
                if (_birthTimer.advance(dt)) {
                    if (_particles.size() < kMaxParticleCount) {
                        doSpawnParticle();
                    }
                    _birthTimer.reset(_birthInterval);
                }
            }
            break;
        case Emitter::UpdateType::Single:
            if (!_spawned || (_particles.empty() && _emitter->loop())) {
                doSpawnParticle();
                _spawned = true;
            }
            break;
        default:
            break;
    }
}

void EmitterSceneNode::doSpawnParticle() {
    float halfW = 0.005f * _emitter->size().x;
    float halfH = 0.005f * _emitter->size().y;
    glm::vec3 position(random(-halfW, halfW), random(-halfH, halfH), 0.0f);

    float sign;
    if (_emitter->spread() > glm::pi<float>() && random(0, 1) != 0) {
        sign = -1.0f;
    } else {
        sign = 1.0f;
    }
    float velocity = sign * (_emitter->velocity() + random(0.0f, _emitter->randomVelocity()));

    auto particle = make_shared<ParticleSceneNode>(_modelSceneNode, position, velocity, _emitter, _sceneGraph);
    _particles.push_back(particle);
    addChild(particle);
}

void EmitterSceneNode::removeExpiredParticles(float dt) {
    for (auto it = _particles.begin(); it != _particles.end(); ) {
        auto &particle = (*it);
        if (particle->isExpired()) {
            removeChild(*particle);
            it = _particles.erase(it);
        } else {
            ++it;
        }
    }
}

void EmitterSceneNode::detonate() {
    doSpawnParticle();
}

} // namespace scene

} // namespace reone
