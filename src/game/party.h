#ifndef FALLOUT_GAME_PARTY_H_
#define FALLOUT_GAME_PARTY_H_

#include <stdbool.h>

#include "game/object_types.h"
#include "plib/db/db.h"

int partyMemberAdd(Object* object);
int partyMemberRemove(Object* object);
int partyMemberPrepSave();
int partyMemberUnPrepSave();
int partyMemberSave(DB_FILE* stream);
int partyMemberPrepLoad();
int partyMemberRecoverLoad();
int partyMemberLoad(DB_FILE* stream);
void partyMemberClear();
int partyMemberSyncPosition();
int partyMemberRestingHeal(int a1);
Object* partyMemberFindObjFromPid(int pid);
bool isPartyMember(Object* object);
int getPartyMemberCount();
int partyMemberPrepItemSaveAll();

#endif /* FALLOUT_GAME_PARTY_H_ */
