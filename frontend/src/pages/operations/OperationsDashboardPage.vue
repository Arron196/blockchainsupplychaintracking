<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from "vue";

import { fetchMetricsOverview } from "@/api/metrics";
import EventFeed from "@/components/EventFeed.vue";
import MetricCard from "@/components/MetricCard.vue";
import type { IngestRejectedEvent, MetricsOverview, TelemetryIngestedEvent } from "@/types/contracts";
import { connectAlertSocket, connectTelemetrySocket } from "@/websocket/client";

const metrics = ref<MetricsOverview | null>(null);
const loading = ref(true);
const loadError = ref<string | null>(null);
const telemetryUpdates = ref<TelemetryIngestedEvent[]>([]);
const alertUpdates = ref<IngestRejectedEvent[]>([]);

let telemetrySocket: WebSocket | null = null;
let alertSocket: WebSocket | null = null;

const telemetryItems = computed(() =>
  telemetryUpdates.value
    .slice(0, 8)
    .map(
      (event) =>
        `${event.deviceId} accepted #${event.recordId} via ${event.transport} (${event.txHash || "pending tx"})`
    )
);

const alertItems = computed(() =>
  alertUpdates.value.slice(0, 8).map((event) => `${event.deviceId} rejected: ${event.message}`)
);

const loadMetrics = async (): Promise<void> => {
  loading.value = true;
  loadError.value = null;

  try {
    metrics.value = await fetchMetricsOverview();
  } catch (error) {
    loadError.value = error instanceof Error ? error.message : "Failed to load metrics overview.";
  } finally {
    loading.value = false;
  }
};

onMounted(async () => {
  await loadMetrics();

  telemetrySocket = connectTelemetrySocket({
    onMessage: (event) => {
      telemetryUpdates.value = [event, ...telemetryUpdates.value].slice(0, 20);
    }
  });

  alertSocket = connectAlertSocket({
    onMessage: (event) => {
      alertUpdates.value = [event, ...alertUpdates.value].slice(0, 20);
    }
  });
});

onUnmounted(() => {
  telemetrySocket?.close();
  alertSocket?.close();
});
</script>

<template>
  <section class="dashboard-grid">
    <div class="panel headline-panel">
      <h2>Gateway Status Visibility</h2>
      <p class="muted">Live counters from <code>/api/v1/metrics/overview</code> with websocket event channels attached.</p>
      <button class="btn" type="button" @click="loadMetrics">Refresh snapshot</button>
      <p v-if="loadError" class="error">{{ loadError }}</p>
    </div>

    <template v-if="metrics && !loading">
      <MetricCard label="Total Requests" :value="metrics.totalRequests" />
      <MetricCard label="Accepted" :value="metrics.acceptedRequests" />
      <MetricCard label="Rejected" :value="metrics.rejectedRequests" />
      <MetricCard label="Avg Processing (ms)" :value="metrics.averageProcessingMs.toFixed(1)" />
      <MetricCard label="Records Stored" :value="metrics.repositorySize" />
    </template>

    <div v-else class="panel">Loading metrics snapshot...</div>

    <EventFeed
      title="Telemetry Stream (/ws/telemetry)"
      :items="telemetryItems"
      empty-text="No accepted ingest event seen in this session."
    />

    <EventFeed
      title="Alert Stream (/ws/alerts)"
      :items="alertItems"
      empty-text="No rejected ingest event seen in this session."
    />
  </section>
</template>

<style scoped>
.dashboard-grid {
  display: grid;
  gap: 0.9rem;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
}

.headline-panel {
  grid-column: 1 / -1;
}

h2 {
  margin: 0;
}

.muted {
  color: var(--ink-muted);
}

.btn {
  border: 1px solid var(--line-strong);
  border-radius: 10px;
  background: linear-gradient(160deg, #2f5f56, #245278);
  color: var(--ink);
  padding: 0.45rem 0.8rem;
  cursor: pointer;
}

.error {
  color: var(--danger);
}
</style>
