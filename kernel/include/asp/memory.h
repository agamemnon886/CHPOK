/* Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 */
#ifndef POK_MEM_H
#define POK_MEM_H

/* TODO: Whole header should be removed when functionality becomes not needed. */

/* 
 * Return physical address for given user space address.
 * 
 * Return 0 if memory cannot be accessed by the user.
 */
uintptr_t pok_virt_to_phys(uintptr_t virt);
/* 
 * Return user space address for given physical address.
 * 
 * Return 0 if memory cannot be accessed by the user.
 */
uintptr_t pok_phys_to_virt(uintptr_t phys);

#endif
