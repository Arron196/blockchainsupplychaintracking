<script setup lang="ts">
import type { TelemetryRecord } from "@/types/contracts";

defineProps<{
  records: TelemetryRecord[];
}>();

const renderTime = (timestamp: number): string => {
  const normalized = timestamp > 10_000_000_000 ? timestamp : timestamp * 1000;
  return new Date(normalized).toLocaleString();
};
</script>

<template>
  <div class="panel records-wrap">
    <table>
      <thead>
        <tr>
          <th>Record</th>
          <th>Device</th>
          <th>Timestamp</th>
          <th>Tx Hash</th>
          <th>Transport</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="record in records" :key="record.recordId">
          <td>#{{ record.recordId }}</td>
          <td>{{ record.deviceId }}</td>
          <td>{{ renderTime(record.timestamp) }}</td>
          <td class="mono">{{ record.receipt?.txHash || "not submitted" }}</td>
          <td>{{ record.transport }}</td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<style scoped>
.records-wrap {
  overflow-x: auto;
}

table {
  width: 100%;
  border-collapse: collapse;
  min-width: 700px;
}

th,
td {
  text-align: left;
  padding: 0.6rem;
  border-bottom: 1px solid var(--line-soft);
  color: var(--ink);
}

th {
  color: var(--ink-muted);
  font-size: 0.76rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
}

.mono {
  font-family: var(--font-mono);
  font-size: 0.78rem;
}
</style>
