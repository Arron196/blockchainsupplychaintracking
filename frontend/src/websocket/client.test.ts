import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

class MockWebSocket {
  public static readonly instances: MockWebSocket[] = [];

  public static readonly OPEN = 1;

  public readonly url: string;

  public readyState = 0;

  private readonly listeners = new Map<string, Array<(event: any) => void>>();

  public constructor(url: string) {
    this.url = url;
    MockWebSocket.instances.push(this);
  }

  public addEventListener(type: string, callback: (event: any) => void): void {
    const current = this.listeners.get(type) ?? [];
    current.push(callback);
    this.listeners.set(type, current);
  }

  public close(): void {
    this.readyState = 3;
    this.dispatch("close", { type: "close" });
  }

  public dispatch(type: string, event: any): void {
    for (const callback of this.listeners.get(type) ?? []) {
      callback(event);
    }
  }
}

describe("websocket client", () => {
  beforeEach(() => {
    vi.resetModules();
    MockWebSocket.instances.length = 0;
    vi.stubGlobal("WebSocket", MockWebSocket as unknown as typeof WebSocket);
  });

  afterEach(() => {
    vi.unstubAllGlobals();
    vi.restoreAllMocks();
  });

  it("converts default API base to websocket url", async () => {
    const { connectTelemetrySocket } = await import("@/websocket/client");

    connectTelemetrySocket({ onMessage: () => {} });

    expect(MockWebSocket.instances[0]?.url).toBe("ws://127.0.0.1:8080/ws/telemetry");
  });

  it("emits parse and error details through onError", async () => {
    const onError = vi.fn();
    const onMessage = vi.fn();

    const { connectAlertSocket } = await import("@/websocket/client");
    connectAlertSocket({ onMessage, onError });

    const socket = MockWebSocket.instances[0];
    socket.dispatch("message", { data: "not-json" });
    socket.dispatch("message", {
      data: JSON.stringify({
        type: "telemetry.ingested",
        deviceId: "stm32-1",
        recordId: 1,
        timestamp: 1,
        transport: "wifi",
        txHash: "0xabc"
      })
    });
    socket.dispatch("error", { type: "error" });

    expect(onMessage).not.toHaveBeenCalled();
    expect(onError).toHaveBeenCalledTimes(3);
    expect(onError.mock.calls[0]?.[0].message).toContain("Failed to parse websocket payload");
    expect(onError.mock.calls[1]?.[0].message).toContain("payload shape is invalid");
    expect(onError.mock.calls[2]?.[0].message).toContain("targetUrl");
  });
});
