<script setup lang="ts">
defineProps<{
  title: string;
  emptyText: string;
  items: string[];
}>();
</script>

<template>
  <section class="panel event-feed">
    <h3>{{ title }}</h3>
    <ul v-if="items.length > 0">
      <li v-for="(item, index) in items" :key="`${index}-${item}`">{{ item }}</li>
    </ul>
    <p v-else class="muted">{{ emptyText }}</p>
  </section>
</template>

<style scoped>
.event-feed h3 {
  margin: 0;
}

.event-feed ul {
  margin: 0.85rem 0 0;
  padding-left: 1rem;
  display: grid;
  gap: 0.4rem;
}

.event-feed li {
  color: var(--ink-muted);
  font-family: var(--font-mono);
  font-size: 0.82rem;
}

.muted {
  margin: 0.85rem 0 0;
  color: var(--ink-muted);
}
</style>
