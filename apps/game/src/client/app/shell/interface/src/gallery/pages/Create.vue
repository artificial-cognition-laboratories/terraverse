<script setup lang="ts">
import CreateWorld from "../../menu/create/CreateWorld.vue"
import { CANNED_HOME } from "../mock.ts"
</script>

<template>
    <CreateWorld :home="CANNED_HOME" />
</template>
