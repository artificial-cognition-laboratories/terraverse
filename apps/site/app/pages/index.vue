<template>
    <div>
        <home-hero />
        <home-features />
        <shared-footer />
    </div>
</template>

<script setup lang="ts">
definePageMeta({ layout: "landing" })
useHead({ title: "" })
</script>
