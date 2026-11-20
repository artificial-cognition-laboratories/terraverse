import type { InjectionKey, Ref } from "vue"

/** Search belongs to a workbench's navigator, including nested workspaces. */
export type WorkbenchSearch = {
    query: Ref<string>
    label: Ref<string | undefined>
    placeholder: Ref<string | undefined>
}

export const WORKBENCH_SEARCH: InjectionKey<WorkbenchSearch> = Symbol("workbench-search")
