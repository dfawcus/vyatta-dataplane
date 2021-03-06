/*
 * Copyright (c) 2018-2020, AT&T Intellectual Property.  All rights reserved.
 * Copyright (c) 2011-2016 by Brocade Communications Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef _ALG_H_
#define _ALG_H_

typedef struct npf_alg npf_alg_t;

#include <netinet/in.h>
#include <rte_atomic.h>
#include <rte_mbuf.h>
#include <rte_spinlock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <urcu.h>

#include "compiler.h"
#include "npf/npf.h"
#include "npf/npf_cache.h"
#include "npf/npf_session.h"
#include "urcu.h"
#include "util.h"
#include "npf/alg/alg_apt.h"
#include "npf/alg/sip/sip.h"

/* Forward Declarations */
struct npf_session;
struct ifnet;
struct npf_alg;
struct rte_mbuf;
struct npf_nat;


/* ALG Nat */
struct npf_alg_nat {
	npf_addr_t		an_oaddr;
	npf_addr_t		an_taddr;
	in_port_t		an_oport;
	in_port_t		an_tport;
	uint32_t		an_flags;
	vrfid_t			an_vrfid;
};

/* ALG Instance */
struct npf_alg_instance {
	struct apt_instance		*ai_apt;
	uint32_t			ai_vrfid;
	uint32_t			ai_ref_count;
	struct npf_alg			*ai_ftp;
	struct npf_alg			*ai_tftp;
	struct npf_alg			*ai_sip;
	struct npf_alg			*ai_rpc;
};

/* ALG operations struct */
struct npf_alg_ops {
	int		(*se_init)(npf_session_t *, npf_cache_t *,
				   struct apt_tuple *, const int di);
	void		(*se_destroy)(npf_session_t *se);
	void		(*se_expire)(npf_session_t *se);
	void		(*inspect)(npf_session_t *, npf_cache_t *,
				struct rte_mbuf *, struct ifnet *, int);
	int		(*config)(struct npf_alg *, int type, int argc,
				char *const argv[]);
	int		(*reset)(struct npf_alg *, bool);
	void		(*nat_inspect)(npf_session_t *, npf_cache_t *,
				struct npf_nat *, int);
	int		(*nat_in)(npf_session_t *, npf_cache_t *,
				struct rte_mbuf *, struct npf_nat *);
	int		(*nat_out)(npf_session_t *, npf_cache_t *,
				 struct rte_mbuf *, struct npf_nat *);
	void		(*periodic)(struct npf_alg *);
	void		(*tuple_delete)(struct apt_tuple *at);
	const char	*name;
};

#define alg_has_op(a, o) ((a) && (a)->na_ops && (a)->na_ops->o)

/* A default config item */
struct npf_alg_config_item {
	uint8_t         ci_proto;
	uint32_t        ci_flags;
	uint32_t        ci_alg_flags;
	uint32_t	ci_datum;	/* port/etc */
};

/* config item handler */
typedef int (*config_item_handler_t)(struct npf_alg *, int,
					const struct npf_alg_config_item *);

/* Struct for a default configuration */
#define NPF_ALG_MAX_CONFIG	2
struct npf_alg_config {
	uint16_t			ac_cli_refcnt;
	bool				ac_default_set;
	uint8_t				ac_item_cnt;
	config_item_handler_t		ac_handler;
	const struct npf_alg_config_item *ac_items;
};

/* ALG ID */
enum npf_alg_id {
	NPF_ALG_ID_FTP = 1,
	NPF_ALG_ID_TFTP,
	NPF_ALG_ID_RPC,
	NPF_ALG_ID_SIP,
};

/* ALG application instance struct */
struct npf_alg {
	enum npf_alg_id			na_id;
	bool				na_enabled;
	void				*na_private;
	const struct npf_alg_ops	*na_ops;
	struct npf_alg_instance		*na_ai;
	struct apt_instance		*na_ai_apt;
	rte_atomic32_t			na_refcnt;
	uint8_t				na_num_configs;
	struct npf_alg_config		na_configs[NPF_ALG_MAX_CONFIG];
};

/* 'struct npf_session' s_alg handle */
struct npf_session_alg {
	const struct npf_alg	*sa_alg;	/* ALG handle */
	void			*sa_private;	/* ALG private data */
	uint32_t		sa_flags;	/* For ALG usage */
	bool			sa_inspect;	/* inspect packets? */
};


/* ALG names */
#define NPF_ALG_FTP_NAME	"ftp"
#define NPF_ALG_TFTP_NAME	"tftp"
#define NPF_ALG_RPC_NAME	"rpc"
#define NPF_ALG_SIP_NAME	"sip"

/* ALG config ops */
#define NPF_ALG_CONFIG_SET      1
#define NPF_ALG_CONFIG_DELETE   2
#define NPF_ALG_CONFIG_ENABLE   3
#define NPF_ALG_CONFIG_DISABLE  4

const char *npf_alg_id2name(enum npf_alg_id id);

struct apt_tuple *alg_lookup_every_table(const struct ifnet *ifp,
					 const npf_cache_t *npc);

/* Convenience macros to get various instances from an alg instance */
#define alg_to_alg_inst(a)	((a)->na_ai)
#define alg_to_npf_inst(a)	(alg_to_alg_inst(a)->ai_ni)

/* 'struct npf_session_alg' accessors */
void npf_alg_session_set_private(struct npf_session *se, void *data);
void *npf_alg_session_get_private(const struct npf_session *se);
void *npf_alg_session_get_and_set_private(const npf_session_t *se, void *data);

int npf_alg_session_test_flag(const struct npf_session *se, uint32_t flag);
void npf_alg_session_set_flag(struct npf_session *se, uint32_t flag);
uint32_t npf_alg_session_get_flags(const struct npf_session *se);
bool npf_alg_session_inspect(struct npf_session *se);
void npf_alg_session_set_inspect(struct npf_session *se, bool v);
int npf_alg_session_set_alg(struct npf_session *se, const struct npf_alg *alg);
struct npf_alg *npf_alg_session_get_alg(const struct npf_session *se);

struct apt_tuple *alg_search_all_then_any_sport(struct npf_alg_instance *ai,
						struct npf_cache *npc,
						uint32_t ifx);

/* Protos */
int npf_alg_register(struct npf_alg *np);
void alg_reset_instance(struct vrf *vrf, struct npf_alg_instance *ai,
			bool hard);
int npf_alg_manage_config_item(struct npf_alg *na, struct npf_alg_config *ac,
			       int op, struct npf_alg_config_item *ci);
int npf_alg_port_handler(struct npf_alg *na, int op,
			 const struct npf_alg_config_item *ci);
int npf_alg_session_nat(npf_session_t *se, struct npf_nat *nat,
			npf_cache_t *npc, const int di, struct apt_tuple *nt,
			struct npf_alg_nat *an);
int npf_alg_reserve_translations(npf_session_t *se, int nr_ports,
				 bool start_even, uint8_t alen,
				 npf_addr_t *addr, in_port_t *port);
int npf_alg_free_translation(npf_session_t *se, npf_addr_t *addr,
			     in_port_t port);
void npf_alg_destroy_alg(struct npf_alg *alg);
struct npf_alg *npf_alg_create_alg(struct npf_alg_instance *ai,
				   enum npf_alg_id id);
void npf_alg_timer_init(void);
void npf_alg_timer_uninit(void);
void npf_alg_timer_reset(void);
void alg_expire_session_tuples(const struct npf_alg *alg, npf_session_t *se);
struct apt_tuple *alg_lookup_npc(struct npf_alg_instance *ai,
				 npf_cache_t *npc, uint32_t ifx);

struct npf_nat *npf_alg_parent_nat(npf_session_t *se);

int alg_dump(struct npf_alg_instance *ai, vrfid_t vrfid,
	     json_writer_t *json);
int npf_alg_config(uint32_t ext_vrfid, const char *name, int op, int argc,
		   char **argv);
int npf_alg_state_set(uint32_t ext_vrfid, const char *name, int op);

struct npf_alg *npf_alg_tftp_create_instance(struct npf_alg_instance *ai);
void npf_alg_tftp_destroy_instance(struct npf_alg *na);

struct npf_alg *npf_alg_ftp_create_instance(struct npf_alg_instance *ai);
void npf_alg_ftp_destroy_instance(struct npf_alg *na);

struct npf_alg *npf_alg_sip_create_instance(struct npf_alg_instance *ai);
void npf_alg_sip_destroy_instance(struct npf_alg *na);

struct npf_alg *npf_alg_rpc_create_instance(struct npf_alg_instance *ai);
void npf_alg_rpc_destroy_instance(struct npf_alg *na);

void npf_alg_flush_all(void);

#endif /* End of _ALG_H_ */
