import { afterEach, describe, expect, it, vi } from "vitest";

import { API_BASE_URL } from "@/api/http";
import { fetchBatchTrace } from "@/api/traceability";

describe("fetchBatchTrace", () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it("requests backend trace route with encoded batch code", async () => {
    const fakeResponse = {
      batchCode: "LOT-2026/03",
      count: 0,
      records: []
    };

    const fetchSpy = vi.spyOn(globalThis, "fetch").mockResolvedValue({
      ok: true,
      status: 200,
      text: async () => JSON.stringify(fakeResponse)
    } as Response);

    await fetchBatchTrace("LOT-2026/03");

    expect(fetchSpy).toHaveBeenCalledWith(`${API_BASE_URL}/api/v1/batches/LOT-2026%2F03/trace`, {
      method: "GET",
      headers: {
        Accept: "application/json"
      }
    });
  });
});
