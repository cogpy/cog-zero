/*
 * cog0_atomspace_bridge.h
 *
 * Bridge between cog0's standalone AtomStore and OpenCog's AtomSpace.
 * Provides bidirectional sync: cog0 atoms ↔ OpenCog atoms.
 * Also exposes the NL↔Atomese pipeline through the cog0 cognitive loop.
 *
 * Build: links against both libcog0 (standalone) and libatomspace (OpenCog)
 */
#ifndef COG0_ATOMSPACE_BRIDGE_H
#define COG0_ATOMSPACE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge lifecycle */
typedef void* cog0_bridge_t;

cog0_bridge_t cog0_bridge_create(void);
void cog0_bridge_destroy(cog0_bridge_t bridge);

/* NL → Atomese (returns Scheme s-expression string, caller must free()) */
char* cog0_bridge_nl_to_atomese(cog0_bridge_t bridge, const char* text);

/* Atomese → NL (returns English string, caller must free()) */
char* cog0_bridge_atomese_to_nl(cog0_bridge_t bridge, const char* atomese);

/* Inject a percept into the cognitive loop and run N cycles */
void cog0_bridge_percept_and_run(cog0_bridge_t bridge, const char* text, int cycles);

/* Sync cog0 AtomStore → OpenCog AtomSpace (Scheme eval on CogServer) */
int cog0_bridge_sync_to_cogserver(cog0_bridge_t bridge, const char* host, int port);

/* Query the bridge's internal atom count */
int cog0_bridge_atom_count(cog0_bridge_t bridge);

/* Get status report (caller must free()) */
char* cog0_bridge_status(cog0_bridge_t bridge);

/* Version */
const char* cog0_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* COG0_ATOMSPACE_BRIDGE_H */
