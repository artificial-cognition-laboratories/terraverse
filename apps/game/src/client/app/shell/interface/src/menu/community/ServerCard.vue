<script setup lang="ts">
import type { PublicServer } from "../../bridge/index.ts"
import { Card } from "../../kit/index.ts"

/** A public server as a card: its players, ping and region. */
defineProps<{ server: PublicServer }>()
defineEmits<{ open: [] }>()
</script>

<template>
    <Card :title="server.name" :seed="server.id" :by="`hosted by ${server.host}`" :description="server.description"
        :badges="server.featured ? ['Featured'] : []" dense @open="$emit('open')">
        <template #meta>
            <span class="live">● {{ server.players }}/{{ server.capacity }}</span>
            <span v-if="server.pingMs !== null">{{ server.pingMs }} ms</span>
            <span>{{ server.region }}</span>
            <span v-if="!server.compatible" class="old">Other version</span>
        </template>
    </Card>
</template>

<style scoped>
.live { color: #6fcf8a; }
.old { margin-left: auto; color: #e0b33d; }
</style>
