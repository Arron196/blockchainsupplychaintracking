import { afterEach, describe, expect, it, vi } from "vitest";

import { API_BASE_URL, ApiError, fetchJson, toApiUrl } from "@/api/http";

describe("http api helpers", () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it("builds api urls against configured base", () => {
    expect(toApiUrl("/health")).toBe(`${API_BASE_URL}/health`);
  });

  it("returns parsed json when response is ok", async () => {
    const payload = { status: "ok" };
    const fetchSpy = vi.spyOn(globalThis, "fetch").mockResolvedValue({
      ok: true,
      status: 200,
      text: async () => JSON.stringify(payload)
    } as Response);

    const result = await fetchJson<{ status: string }>("/health");
    expect(result).toEqual(payload);
    expect(fetchSpy).toHaveBeenCalledWith(`${API_BASE_URL}/health`, {
      method: "GET",
      headers: {
        Accept: "application/json"
      }
    });
  });

  it("throws ApiError with status and body for non-2xx", async () => {
    vi.spyOn(globalThis, "fetch").mockResolvedValue({
      ok: false,
      status: 503,
      text: async () => "gateway unavailable"
    } as Response);

    await expect(fetchJson("/health")).rejects.toEqual(
      expect.objectContaining<ApiError>({
        name: "ApiError",
        status: 503,
        responseBody: "gateway unavailable"
      })
    );
  });
});
