@ Must carefully read B2 of the armv6 manual.  You cannot rely on "well it
@ worked on my test case": mistakes cause stale data, which may or may not
@ show up in your test case (likely not) despite being wildly broken.
@
@ Note: Rd is allegedly a read-only parameter to these instructions, but
@ the manual says SBZ (should be zero).  I think (and the linux source seems
@ to think) that this means we have to explicitly set it to 0.  Not setting
@ Rd=0 is an easy mistake to make.  Perhaps is worth building in (Linux does
@ not, but) --- the cost of an extra mov is negligible given our other 
@ overheads.
@
@ Alot of people put these instructions in inline assembly, wrapped up by 
@ function calls (e.g., cs107e's MMU code).  This practice is dangerous. 
@ For example, if you are flushing caches, you cannot be sure that the 
@ function return, etc does not then reload them, esp w.r.t. the BTB.  I 
@ think the only safe approach is to put directly in assembly so you are 
@ guaranteed no branch, indirect jump, load/store happens in between.
@
@ A lot of MMU code out there appears pretty broken b/c they don't do enough
@ flushing operations and/or rely on gcc code generation to not do the above.

@ used to clear register before CP15 operation.
#define CLR(reg) mov reg, #0 

@ used to be called "drain write buffer"
@ includes all cache operations.  is a superset of
@ > DMB
#define DSB(Rd)             mcr p15, 0, Rd, c7, c10, 4

#define DMB(Rd)             mcr p15, 0, Rd, c7, c10, 5 

@ must flush the prefetch buffer whenever you change a virtual 
@ mapping (ASID, PTE, etc) since it will have stale instructions.
@
@ if you are doing this, likely have to do a DSB before to make
@ sure whatever invalidation you did has completed.
#define ISB(Rd) PREFETCH_FLUSH(Rd)
#define PREFETCH_FLUSH(Rd)  mcr p15, 0, Rd, c7, c5, 4  

@ must do this after changing any MMU stuff, ASID, etc.
#define FLUSH_BTB(Rd)         mcr p15, 0, Rd, c7, c5, 6

@ need to figure out if have a unified or separate I/D cache/TLB

@ Work-around for bug in ARMv6 if we have seperate I/D.  Taken from:
@   https://elixir.bootlin.com/linux/latest/source/arch/arm/mm/cache-v6.S
@ MUST HAVE INTERRUPTS DISABLED!
@ XXX: patch feedback implies we need this for other operations too?
#define INV_ICACHE(Rd)                                           \
    mov Rd, #0                  ;                                   \
    mcr p15, 0, Rd, c7, c5, 0   ; /* invalidate entire I-cache */   \
    mcr p15, 0, Rd, c7, c5, 0;  ; /* invalidate entire I-cache */   \
    mcr p15, 0, Rd, c7, c5, 0;  ; /* invalidate entire I-cache */   \
    mcr p15, 0, Rd, c7, c5, 0;  ; /* invalidate entire I-cache */   \
    .rept   11                  ; /* ARM Ltd recommends at least 11 nops */\
    nop                         ;                                   \
    .endr                     

#define INV_DCACHE(Rd)      mcr p15, 0, Rd, c7, c6, 0  
#define INV_ALL_CACHES(Rd)  mcr p15, 0, Rd, c7, c7, 0  

@ invalidate TLB entries.
#define INV_ITLB(Rd)        mcr p15, 0, Rd, c8, c5, 0 
#define INV_DTLB(Rd)        mcr p15, 0, Rd, c8, c6, 0 
@ invalidate unified TLB or both I/D TLB
#define INV_TLB(Rd)         mcr p15, 0, Rd, c8, c7, 0

#define SET_ASID(Rd)  mcr p15, 0, Rd, c13, c0, 1
#define SET_TTBR0(Rd)  mcr p15, 0, Rd, c2, c0, 0
#define GET_TTBR0(Rd)  mrc p15, 0, Rd, c2, c0, 0
#define SET_TTBR1(Rd)   mcr p15, 0, Rd, c2, c0, 1

.globl get_cache_type
get_cache_type:
    @@@@  implement @@@@
    mrc p15, 0, r0, c0, c0, 1 
    bx lr

@ B6-21 tells us the c-numbers we must use
@ Write to a 2-bit field in the domain access control register
.globl write_domain_access_ctrl
write_domain_access_ctrl:
    mcr p15, 0, r0, c3, c0, 0   @ Write domain access cntl register
    mov r1, #0
    DSB(r1)                     @ ARM manual B2-24: Any write to cp15 requires a prefetch flush
    PREFETCH_FLUSH(r1)
    bx lr

@ B2-25 has the cookbook
@ Change ASID to 0
@ PrefetchFlush
@ Change Translation Table Base Register 
@ PrefetchFlush
@ Change ASID to new value
@ Line number for setting ASID: B4-52
.globl set_procid_ttbr0
set_procid_ttbr0:
    CLR(r2)
    SET_ASID(r2)        @ Given procid, set its ASID to 0 first
    PREFETCH_FLUSH(r2)

    SET_TTBR0(r1)               @ B4-41: set translation table base to our pointer
    SET_TTBR1(r2)               @ Set TTBR1 to 0
    mcr p15, 0, r2, c2, c0, 2   @ Set TTBRC to 0
    
    PREFETCH_FLUSH(r2)

    SET_ASID(r0) 

    @ BTB is the Branch Target Buffer
    @ Note: might not have to do this, see last paragraph in B2-25
    DSB(r2)                     @ B2-25 Must invalidate BTB after changes to TTB
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)          @ B2-24 Must do a prefetch flush after invalidating BTB

    bx lr


.globl mmu_enable
mmu_enable:
    CLR(r1)
                        @ Disable instruction cache (???)
    INV_ICACHE(r1)      @ Invalidate the instruction cache
    DSB(r1)
    FLUSH_BTB(r1)
    DSB(r1)

    @ mrc p15, 0, r2, c1, c0, 0 @ Write domain access cntl register
    @ mov r3, #1
    @ orr r2, r3                @ Or in 1, for M (zeroth) bit 
    mcr p15, 0, r0, c1, c0, 0   @ Enable the MMU by setting bit 0 in the CP15 Control Register (B4-40)
    
    INV_ITLB(r1)
    INV_DTLB(r1)
    INV_TLB(r1)                 @ 6-9 Should invalidate all TLB's 

                                @ Reenable ins. cache (???)
    DSB(r1)                     @ B2-25 Must invalidate BTB after enabling MMU (B2-24)
    FLUSH_BTB(r1)
    DSB(r1)
    PREFETCH_FLUSH(r1)          @ B2-24 Must do a prefetch flush after invalidating BTB
    DSB(r1)
    bx lr

@ 6-9 has the cookbook. 
.globl mmu_disable
mmu_disable:
    CLR(r1)
    INV_DCACHE(r1)              @ Ivalidate the data cache
    DSB(r1)

    mcr p15, 0, r0, c1, c0, 0   @ Disable the MMU by setting bit 0 in the CP15 Control Register (B4-40)

    DSB(r1)                     @ B2-25 Must invalidate BTB after disabling MMU (B2-24)
    FLUSH_BTB(r1)
    DSB(r1)
    PREFETCH_FLUSH(r1)          @ B2-24 Must do a prefetch flush after invalidating BTB
    DSB(r1)
    bx lr