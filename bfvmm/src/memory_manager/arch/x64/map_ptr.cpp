//
// Bareflank Hypervisor
// Copyright (C) 2015 Assured Information Security, Inc.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#include <memory_manager/arch/x64/map_ptr.h>
#include <memory_manager/arch/x64/root_page_table.h>

namespace bfvmm
{
namespace x64
{

void
WEAK_SYM map_with_cr3(
    uintptr_t vmap,
    uintptr_t virt,
    uintptr_t cr3,
    size_t size,
    ::x64::msrs::value_type pat)
{
    uintptr_t pml4_addr = bfn::upper(cr3);

    expects(vmap != 0);
    expects(bfn::lower(vmap) == 0);
    expects(virt != 0);
    expects(pml4_addr != 0);
    expects(size != 0);

    for (auto offset = 0UL; offset < size; offset += ::x64::page_size) {
        uintptr_t from;
        uintptr_t phys;
        uintptr_t pati;
        uintptr_t current_virt = virt + offset;

        while (true) {
            from = ::x64::page_table::pml4::from;
            auto pml4_idx = ::x64::page_table::index(current_virt, from);
            auto pml4_map = bfvmm::x64::make_unique_map<uintptr_t>(pml4_addr);
            auto pml4_pte = bfvmm::x64::page_table_entry{&pml4_map.get()[pml4_idx]};

            expects(pml4_pte.present());
            expects(pml4_pte.phys_addr() != 0);

            from = ::x64::page_table::pdpt::from;
            auto pdpt_idx = ::x64::page_table::index(current_virt, from);
            auto pdpt_map = bfvmm::x64::make_unique_map<uintptr_t>(pml4_pte.phys_addr());
            auto pdpt_pte = bfvmm::x64::page_table_entry{&pdpt_map.get()[pdpt_idx]};

            expects(pdpt_pte.present());
            expects(pdpt_pte.phys_addr() != 0);

            if (pdpt_pte.ps()) {
                phys = pdpt_pte.phys_addr();
                pati = pdpt_pte.pat_index_large();
                break;
            }

            from = ::x64::page_table::pd::from;
            auto pd_idx = ::x64::page_table::index(current_virt, from);
            auto pd_map = bfvmm::x64::make_unique_map<uintptr_t>(pdpt_pte.phys_addr());
            auto pd_pte = bfvmm::x64::page_table_entry{&pd_map.get()[pd_idx]};

            expects(pd_pte.present());
            expects(pd_pte.phys_addr() != 0);

            if (pd_pte.ps()) {
                phys = pd_pte.phys_addr();
                pati = pd_pte.pat_index_large();
                break;
            }

            from = ::x64::page_table::pt::from;
            auto pt_idx = ::x64::page_table::index(current_virt, from);
            auto pt_map = bfvmm::x64::make_unique_map<uintptr_t>(pd_pte.phys_addr());
            auto pt_pte = bfvmm::x64::page_table_entry{&pt_map.get()[pt_idx]};

            expects(pt_pte.present());
            expects(pt_pte.phys_addr() != 0);

            phys = pt_pte.phys_addr();
            pati = pt_pte.pat_index_4k();
            break;
        }

        auto vadr = vmap + offset;
        auto padr = bfn::upper(phys, from) | bfn::lower(current_virt, from);

        auto perm = ::x64::memory_attr::rw;
        auto type = ::x64::msrs::ia32_pat::pa(pat, pati);

        g_pt->map_4k(vadr, bfn::upper(padr), ::x64::memory_attr::mem_type_to_attr(perm, type));
    }
}

}
}
