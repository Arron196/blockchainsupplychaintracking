import { fetchJson } from "@/api/http";
import type { MetricsOverview } from "@/types/contracts";

export const fetchMetricsOverview = async (): Promise<MetricsOverview> =>
  fetchJson<MetricsOverview>("/api/v1/metrics/overview");
