import { afterEach, describe, expect, it, vi } from "vitest";

import { API_BASE_URL } from "@/api/http";
import { fetchMetricsOverview } from "@/api/metrics";

describe("fetchMetricsOverview", () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it("requests metrics overview endpoint", async () => {
    const payload = {
      totalRequests: 3,
      acceptedRequests: 2,
      rejectedRequests: 1,
      averageProcessingMs: 4,
      repositorySize: 2
    };

    const fetchSpy = vi.spyOn(globalThis, "fetch").mockResolvedValue({
      ok: true,
      status: 200,
      text: async () => JSON.stringify(payload)
    } as Response);

    const result = await fetchMetricsOverview();
    expect(result).toEqual(payload);
    expect(fetchSpy).toHaveBeenCalledWith(`${API_BASE_URL}/api/v1/metrics/overview`, {
      method: "GET",
      headers: {
        Accept: "application/json"
      }
    });
  });
});
