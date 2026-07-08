/*
 * Copyright (c) 2016, BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * definitions guest shared memory device
 */

#ifndef __GUEST_SHM_H
#define __GUEST_SHM_H

#include <stdint.h>

/*
 * Temporary VID definition until the updated <pci/pci_id.h> propogates around
 */
#define PCI_VID_BlackBerry_QNX	0x1C05

#define PCI_DID_QNX_GUEST_SHM	0x0001

/** status of last creation request */
enum guest_shm_status {
	GSS_OK,					/**< creation succeeded */
	GSS_UNKNOWN_FAILURE,	/**< creation failed for an unknown reason */
	GSS_NOMEM,				/**< creation failed due to lack of memory */
	GSS_CLIENT_MAX,			/**< creation failed due to region already being used by the maximum number of guests */
	GSS_ILLEGAL_NAME,		/**< creation failed due to illegal region name */
};

/** Maximum number of clients allowed to connect to a shared memory region */
#define GUEST_SHM_MAX_CLIENTS	16

/** Maximum length allowed for region name */
#define GUEST_SHM_MAX_NAME	32

/** Signature value to verify that vdev is present */
#define GUEST_SHM_SIGNATURE 0x4d534732474d5651


/** Register layout for factory registers */
struct guest_shm_factory {
	uint64_t		signature; /**< == GUEST_SHM_SIGNATURE (R/O) */
	uint64_t		shmem;	/**< Shared memory paddr (R/O) */
	uint32_t		vector;	/**< interrupt vector number (R/O) */
	uint32_t		status; /**< status of last creation (R/O) */
	uint32_t		size;	/**< requested size in 4K pages, write causes creation */
	char			name[GUEST_SHM_MAX_NAME];	/**< name of shared memory region */
};

/** Register layout for a region control page */
struct guest_shm_control {
	uint32_t		status;	/**< lower 16 bits: pending notification bitset, upper 16 bits: current active clients (R/O) */
	uint32_t		idx;	/**< connection index for this client (R/O) */
	uint32_t		notify;	/**< write a bitset of clients to notify */
	uint32_t		detach; /**< write here to detach from the shared memory region */
};


static inline void
guest_shm_create(volatile struct guest_shm_factory *const __factory, unsigned const __size) {
	/* Surround the size assignment with memory barriers so that
	 * the compiler doesn't try to shift the assignment before/after
	 * necessary bits (e.g. setting the name of the region) */
	asm volatile( "" ::: "memory");
	__factory->size = __size;
	asm volatile( "" ::: "memory");
}

#endif

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL: http://svn.ott.qnx.com/product/hypervisor/branches/qh_2.1/apps/hypervisor/demos/shmem-guest/guest_shm.h $ $Rev: 989188 $")
#endif
