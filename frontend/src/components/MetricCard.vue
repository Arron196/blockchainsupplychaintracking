<script setup lang="ts">
defineProps<{
  label: string;
  value: string | number;
}>();
</script>

<template>
  <article class="metric-card panel">
    <p class="metric-label">{{ label }}</p>
    <p class="metric-value">{{ value }}</p>
  </article>
</template>

<style scoped>
.metric-card {
  min-height: 112px;
}

.metric-label {
  margin: 0;
  color: var(--ink-muted);
  letter-spacing: 0.08em;
  text-transform: uppercase;
  font-size: 0.72rem;
}

.metric-value {
  margin: 0.8rem 0 0;
  font-size: 1.8rem;
  font-weight: 650;
  color: var(--ink);
}
</style>
