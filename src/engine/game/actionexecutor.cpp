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

#include "actionexecutor.h"

#include "../common/log.h"
#include "../common/guardutil.h"
#include "../scene/types.h"
#include "../script/execution.h"
#include "../script/executioncontext.h"

#include "action/usefeat.h"
#include "action/useskill.h"
#include "action/wait.h"
#include "enginetype/location.h"
#include "game.h"
#include "object/area.h"
#include "object/creature.h"
#include "object/door.h"
#include "object/placeable.h"
#include "objectconverter.h"

using namespace std;

using namespace reone::scene;
using namespace reone::script;

namespace reone {

namespace game {

static constexpr float kKeepPathDuration = 1000.0f;
static constexpr float kDefaultMaxObjectDistance = 2.0f;
static constexpr float kMaxConversationDistance = 4.0f;
static constexpr float kDistanceWalk = 4.0f;

ActionExecutor::ActionExecutor(Game *game) : _game(game) {
    ensureNotNull(game, "game");
}

void ActionExecutor::executeActions(const shared_ptr<Object> &object, float dt) {
    shared_ptr<Action> action(object->getCurrentAction());
    if (!action) return;

    ActionType type = action->type();
    switch (type) {
        case ActionType::MoveToPoint:
            executeMoveToPoint(object, *static_pointer_cast<MoveToPointAction>(action), dt);
            break;
        case ActionType::MoveToObject:
            executeMoveToObject(object, *static_pointer_cast<MoveToObjectAction>(action), dt);
            break;
        case ActionType::Follow:
            executeFollow(object, *static_pointer_cast<FollowAction>(action), dt);
            break;
        case ActionType::DoCommand:
            executeDoCommand(object, *static_pointer_cast<CommandAction>(action), dt);
            break;
        case ActionType::StartConversation:
            executeStartConversation(object, *static_pointer_cast<StartConversationAction>(action), dt);
            break;
        case ActionType::AttackObject:
            executeAttack(object, static_pointer_cast<ObjectAction>(action), dt);
            break;
        case ActionType::OpenDoor:
            executeOpenDoor(object, *static_pointer_cast<ObjectAction>(action), dt);
            break;
        case ActionType::CloseDoor:
            executeCloseDoor(object, *static_pointer_cast<ObjectAction>(action), dt);
            break;
        case ActionType::OpenContainer:
            executeOpenContainer(object, *static_pointer_cast<ObjectAction>(action), dt);
            break;
        case ActionType::OpenLock:
            executeOpenLock(object, *static_pointer_cast<ObjectAction>(action), dt);
            break;
        case ActionType::JumpToObject:
            executeJumpToObject(object, *static_pointer_cast<ObjectAction>(action), dt);
            break;
        case ActionType::JumpToLocation:
            executeJumpToLocation(object, *static_pointer_cast<LocationAction>(action), dt);
            break;
        case ActionType::PlayAnimation:
            executePlayAnimation(object, static_pointer_cast<PlayAnimationAction>(action), dt);
            break;
        case ActionType::PauseConversation: {
            _game->conversation().pause();
            action->complete();
            break;
        }
        case ActionType::ResumeConversation: {
            _game->conversation().resume();
            action->complete();
            break;
        }
        case ActionType::Wait:
            if (static_pointer_cast<WaitAction>(action)->advance(dt)) {
                action->complete();
            }
            break;
        case ActionType::FollowLeader:
            executeFollowLeader(object, *action, dt);
            break;
        case ActionType::MoveToLocation:
            executeMoveToLocation(object, *static_pointer_cast<MoveToLocationAction>(action), dt);
            break;
        case ActionType::UseSkill: {
            auto useSkillAction = static_pointer_cast<UseSkillAction>(action);
            if (useSkillAction->skill() == SkillType::Security) {
                executeOpenLock(object, *useSkillAction, dt);
            } else {
                action->complete();
            }
            break;
        }
        case ActionType::UseFeat: {
            auto useFeatAction = static_pointer_cast<UseFeatAction>(action);
            executeAttack(object, move(useFeatAction), dt);
            break;
        }
        default:
            debug("ActionExecutor: action not implemented: " + to_string(static_cast<int>(type)), 2);
            action->complete();
            break;
    }
}

void ActionExecutor::executeMoveToPoint(const shared_ptr<Object> &actor, MoveToPointAction &action, float dt) {
    glm::vec3 dest(action.point());

    bool reached = navigateCreature(static_pointer_cast<Creature>(actor), dest, true, 1.0f, dt);
    if (reached) {
        action.complete();
    }
}

void ActionExecutor::executeMoveToObject(const shared_ptr<Object> &actor, MoveToObjectAction &action, float dt) {
    auto object = static_pointer_cast<SpatialObject>(action.object());
    glm::vec3 dest(object->position());
    bool run = action.isRun();
    float distance = action.distance();

    bool reached = navigateCreature(static_pointer_cast<Creature>(actor), dest, run, distance, dt);
    if (reached) {
        action.complete();
    }
}

void ActionExecutor::executeFollow(const shared_ptr<Object> &actor, FollowAction &action, float dt) {
    auto creatureActor = static_pointer_cast<Creature>(actor);
    auto object = static_pointer_cast<SpatialObject>(action.object());
    glm::vec3 dest(object->position());
    float distance2 = creatureActor->getDistanceTo2(glm::vec2(dest));
    bool run = distance2 > kDistanceWalk * kDistanceWalk;

    navigateCreature(creatureActor, dest, run, action.distance(), dt);
}

void ActionExecutor::executeDoCommand(const shared_ptr<Object> &actor, CommandAction &action, float dt) {
    auto context = make_unique<ExecutionContext>(*action.context());
    context->callerId = actor ? actor->id() : kObjectInvalid;

    shared_ptr<ScriptProgram> program(action.context()->savedState->program);
    ScriptExecution(program, move(context)).run();
    action.complete();
}

void ActionExecutor::executeStartConversation(const shared_ptr<Object> &actor, StartConversationAction &action, float dt) {
    auto creatureActor = ObjectConverter::toCreature(actor);
    auto object = static_pointer_cast<SpatialObject>(action.object());

    bool reached =
        !creatureActor ||
        action.isStartRangeIgnored() ||
        navigateCreature(creatureActor, object->position(), true, kMaxConversationDistance, dt);

    if (reached) {
        bool isActorLeader = _game->services().party().getLeader() == actor;
        _game->module()->area()->startDialog(isActorLeader ? object : static_pointer_cast<SpatialObject>(actor), action.dialogResRef());
        action.complete();
    }
}

void ActionExecutor::executeAttack(const shared_ptr<Object> &actor, shared_ptr<ObjectAction> action, float dt) {
    // If target is dead, complete the action
    shared_ptr<SpatialObject> target(static_pointer_cast<SpatialObject>(action->object()));
    if (target->isDead()) {
        action->complete();
        return;
    }

    auto creature = static_pointer_cast<Creature>(actor);

    // Make the actor follow its target. When reached, register an attack
    if (navigateCreature(creature, target->position(), true, action->range(), dt)) {
        _game->services().combat().addAttack(move(creature), move(target), action);
    }
}

bool ActionExecutor::navigateCreature(const shared_ptr<Creature> &creature, const glm::vec3 &dest, bool run, float distance, float dt) {
    if (creature->isMovementRestricted()) return false;

    float distToDest2 = creature->getDistanceTo2(glm::vec2(dest));
    if (distToDest2 <= distance * distance) {
        creature->setMovementType(Creature::MovementType::None);
        creature->clearPath();
        return true;
    }

    bool updatePath = true;
    shared_ptr<Creature::Path> path(creature->path());

    if (path) {
        uint32_t now = SDL_GetTicks();
        if (path->destination == dest || now - path->timeFound <= kKeepPathDuration) {
            advanceCreatureOnPath(creature, run, dt);
            updatePath = false;
        }
    }
    if (updatePath) {
        updateCreaturePath(creature, dest);
    }

    return false;
}

void ActionExecutor::advanceCreatureOnPath(const shared_ptr<Creature> &creature, bool run, float dt) {
    const glm::vec3 &origin = creature->position();
    shared_ptr<Creature::Path> path(creature->path());
    size_t pointCount = path->points.size();
    glm::vec3 dest;
    float distToDest;

    if (path->pointIdx == pointCount) {
        dest = path->destination;
        distToDest = glm::distance2(origin, dest);

    } else {
        const glm::vec3 &nextPoint = path->points[path->pointIdx];
        float distToNextPoint = glm::distance2(origin, nextPoint);
        float distToPathDest = glm::distance2(origin, path->destination);

        if (distToPathDest < distToNextPoint) {
            dest = path->destination;
            distToDest = distToPathDest;
            path->pointIdx = static_cast<int>(pointCount);

        } else {
            dest = nextPoint;
            distToDest = distToNextPoint;
        }
    }

    if (distToDest <= 1.0f) {
        selectNextPathPoint(*path);

    } else if (_game->module()->area()->moveCreatureTowards(creature, dest, run, dt)) {
        creature->setMovementType(run ? Creature::MovementType::Run : Creature::MovementType::Walk);
        creature->setAppliedForce(glm::vec3(glm::normalize(glm::vec2(dest - origin)), 0.0f));

    } else {
        creature->setMovementType(Creature::MovementType::None);
        creature->setAppliedForce(glm::vec3(0.0f));
    }
}

void ActionExecutor::selectNextPathPoint(Creature::Path &path) {
    size_t pointCount = path.points.size();
    if (path.pointIdx < pointCount) {
        path.pointIdx++;
    }
}

void ActionExecutor::updateCreaturePath(const shared_ptr<Creature> &creature, const glm::vec3 &dest) {
    const glm::vec3 &origin = creature->position();
    vector<glm::vec3> points(_game->module()->area()->pathfinder().findPath(origin, dest));
    uint32_t now = SDL_GetTicks();

    creature->setPath(dest, move(points), now);
}

void ActionExecutor::executeOpenDoor(const shared_ptr<Object> &actor, ObjectAction &action, float dt) {
    auto creatureActor = ObjectConverter::toCreature(actor);
    auto door = ObjectConverter::toDoor(action.object());

    bool reached = !creatureActor || navigateCreature(creatureActor, door->position(), true, kDefaultMaxObjectDistance, dt);
    if (reached) {
        bool isObjectSelf = actor == door;
        if (!isObjectSelf && door->isLocked()) {
            string onFailToOpen(door->getOnFailToOpen());
            if (!onFailToOpen.empty()) {
                _game->services().scriptRunner().run(onFailToOpen, door->id(), actor->id());
            }
        } else {
            door->open(actor);
            if (!isObjectSelf) {
                string onOpen(door->getOnOpen());
                if (!onOpen.empty()) {
                    _game->services().scriptRunner().run(onOpen, door->id(), actor->id(), -1);
                }
            }
        }
        action.complete();
    }
}

void ActionExecutor::executeCloseDoor(const shared_ptr<Object> &actor, ObjectAction &action, float dt) {
    auto creatureActor = ObjectConverter::toCreature(actor);
    auto door = ObjectConverter::toDoor(action.object());

    bool reached = !creatureActor || navigateCreature(creatureActor, door->position(), true, kDefaultMaxObjectDistance, dt);
    if (reached) {
        door->close(actor);
        action.complete();
    }
}

void ActionExecutor::executeOpenContainer(const shared_ptr<Object> &actor, ObjectAction &action, float dt) {
    auto creatureActor = static_pointer_cast<Creature>(actor);
    auto placeable = static_pointer_cast<Placeable>(action.object());

    bool reached = navigateCreature(creatureActor, placeable->position(), true, kDefaultMaxObjectDistance, dt);
    if (reached) {
        _game->openContainer(placeable);
        action.complete();
    }
}

void ActionExecutor::executeOpenLock(const shared_ptr<Object> &actor, ObjectAction &action, float dt) {
    shared_ptr<Object> object(action.object());
    if (!object || object->type() != ObjectType::Door) {
        warn("ActionExecutor: unsupported OpenLock object: " + to_string(action.object()->id()));
        action.complete();
        return;
    }

    auto door = static_pointer_cast<Door>(object);
    auto creatureActor = static_pointer_cast<Creature>(actor);

    bool reached = navigateCreature(creatureActor, door->position(), true, kDefaultMaxObjectDistance, dt);
    if (reached) {
        creatureActor->face(*door);
        creatureActor->playAnimation(AnimationType::LoopingUnlockDoor);

        if (!door->isKeyRequired()) {
            door->setLocked(false);
            door->open(actor);

            string onOpen(door->getOnOpen());
            if (!onOpen.empty()) {
                _game->services().scriptRunner().run(onOpen, door->id(), actor->id());
            }
        }

        action.complete();
    }
}

void ActionExecutor::executeJumpToObject(const shared_ptr<Object> &actor, ObjectAction &action, float dt) {
    auto spatialObject = static_pointer_cast<SpatialObject>(action.object());

    auto spatialActor = static_pointer_cast<SpatialObject>(actor);
    spatialActor->setPosition(spatialObject->position());
    spatialActor->setFacing(spatialObject->getFacing());

    action.complete();
}

void ActionExecutor::executeJumpToLocation(const shared_ptr<Object> &actor, LocationAction &action, float dt) {
    auto spatial = static_pointer_cast<SpatialObject>(actor);
    spatial->setPosition(action.location()->position());
    spatial->setFacing(action.location()->facing());

    action.complete();
}

void ActionExecutor::executePlayAnimation(const shared_ptr<Object> &actor, const shared_ptr<PlayAnimationAction> &action, float dt) {
    AnimationProperties properties;
    properties.speed = action->speed();

    auto spatial = static_pointer_cast<SpatialObject>(actor);
    spatial->playAnimation(action->animation(), move(properties), action);
}

void ActionExecutor::executeFollowLeader(const shared_ptr<Object> &actor, Action &action, float dt) {
    auto creatureActor = static_pointer_cast<Creature>(actor);
    glm::vec3 destination(_game->services().party().getLeader()->position());
    float distance2 = creatureActor->getDistanceTo2(glm::vec2(destination));
    bool run = distance2 > kDistanceWalk;

    navigateCreature(creatureActor, destination, run, kDefaultFollowDistance, dt);
}

void ActionExecutor::executeMoveToLocation(const shared_ptr<Object> &actor, MoveToLocationAction &action, float dt) {
    glm::vec3 destination(action.location()->position());

    bool reached = navigateCreature(static_pointer_cast<Creature>(actor), destination, action.isRun(), 1.0f, dt);
    if (reached) {
        action.complete();
    }
}

} // namespace game

} // namespace reone
