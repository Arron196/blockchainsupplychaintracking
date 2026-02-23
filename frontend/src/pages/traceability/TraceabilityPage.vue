<script setup lang="ts">
import { computed, ref } from "vue";

import { fetchBatchTrace } from "@/api/traceability";
import TraceRecordList from "@/components/TraceRecordList.vue";
import type { BatchTraceResponse } from "@/types/contracts";

const batchCodeInput = ref("BATCH-2026-001");
const loading = ref(false);
const errorMessage = ref<string | null>(null);
const result = ref<BatchTraceResponse | null>(null);

const hasRecords = computed(() => (result.value?.records.length ?? 0) > 0);

const submitBatchLookup = async (): Promise<void> => {
  const trimmedCode = batchCodeInput.value.trim();
  if (!trimmedCode) {
    errorMessage.value = "Batch code is required.";
    result.value = null;
    return;
  }

  loading.value = true;
  errorMessage.value = null;
  result.value = null;

  try {
    result.value = await fetchBatchTrace(trimmedCode);
  } catch (error) {
    errorMessage.value = error instanceof Error ? error.message : "Batch query failed.";
  } finally {
    loading.value = false;
  }
};
</script>

<template>
  <section class="trace-layout">
    <div class="panel query-panel">
      <h2>Traceability Query</h2>
      <p class="hint">Queries <code>GET /api/v1/batches/{batchCode}/trace</code> and displays route status for demo proof.</p>

      <form class="query-form" @submit.prevent="submitBatchLookup">
        <label>
          Batch Code
          <input v-model="batchCodeInput" type="text" autocomplete="off" placeholder="BATCH-2026-001" />
        </label>
        <button class="btn" :disabled="loading" type="submit">{{ loading ? "Searching..." : "Query Trace" }}</button>
      </form>

      <p v-if="errorMessage" class="error">{{ errorMessage }}</p>

      <div v-if="result" class="result-summary">
        <p>
          <strong>{{ result.batchCode }}</strong>
          returned
          <strong>{{ result.count }}</strong>
          record(s).
        </p>
      </div>
    </div>

    <TraceRecordList v-if="result && hasRecords" :records="result.records" />
    <div v-else-if="result" class="panel">No records found for this batch code.</div>
  </section>
</template>

<style scoped>
.trace-layout {
  display: grid;
  gap: 0.9rem;
}

h2 {
  margin: 0;
}

.hint {
  color: var(--ink-muted);
}

.query-form {
  margin-top: 0.9rem;
  display: grid;
  gap: 0.7rem;
  grid-template-columns: minmax(220px, 420px) auto;
  align-items: end;
}

label {
  display: grid;
  gap: 0.35rem;
  color: var(--ink-muted);
}

input {
  border: 1px solid var(--line-soft);
  border-radius: 10px;
  background: rgba(14, 26, 40, 0.9);
  color: var(--ink);
  padding: 0.55rem 0.7rem;
  font-size: 0.95rem;
}

.btn {
  border: 1px solid var(--line-strong);
  border-radius: 10px;
  background: linear-gradient(160deg, #22625a, #2d4980);
  color: var(--ink);
  padding: 0.55rem 0.85rem;
  cursor: pointer;
}

.btn:disabled {
  opacity: 0.65;
  cursor: wait;
}

.error {
  color: var(--danger);
}

.result-summary {
  margin-top: 0.7rem;
  color: var(--ink);
}

@media (max-width: 760px) {
  .query-form {
    grid-template-columns: 1fr;
  }
}
</style>
