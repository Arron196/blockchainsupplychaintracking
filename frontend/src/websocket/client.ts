import { API_BASE_URL } from "@/api/http";
import type { IngestRejectedEvent, TelemetryIngestedEvent } from "@/types/contracts";
import { isAlertEvent, isTelemetryEvent, parseDashboardEvent } from "@/websocket/events";

const envWsBase = import.meta.env.VITE_WS_BASE_URL as string | undefined;

const wsBaseUrl = (() => {
  if (envWsBase) {
    return envWsBase.replace(/\/$/, "");
  }
  return API_BASE_URL.replace(/^http(s?):/i, "ws$1:");
})();

interface SocketHandlers<TEvent> {
  onMessage: (event: TEvent) => void;
  onOpen?: () => void;
  onClose?: () => void;
  onError?: (error: Error) => void;
  parse?: (payload: unknown) => TEvent | null;
}

const connectSocket = <TEvent>(path: string, handlers: SocketHandlers<TEvent>): WebSocket => {
  const socket = new WebSocket(`${wsBaseUrl}${path}`);

  socket.addEventListener("open", () => {
    handlers.onOpen?.();
  });

  socket.addEventListener("message", (messageEvent) => {
    try {
      const rawPayload: unknown = JSON.parse(messageEvent.data as string);
      const parsed = handlers.parse ? handlers.parse(rawPayload) : (rawPayload as TEvent);
      if (parsed == null) {
        handlers.onError?.(new Error("WebSocket payload shape is invalid."));
        return;
      }
      handlers.onMessage(parsed);
    } catch {
      handlers.onError?.(new Error("Failed to parse websocket payload as JSON."));
      return;
    }
  });

  socket.addEventListener("error", (event) => {
    const detail = JSON.stringify({
      readyState: socket.readyState,
      type: event.type,
      targetUrl: socket.url
    });
    handlers.onError?.(new Error(`WebSocket connection error: ${detail}`));
  });

  socket.addEventListener("close", () => {
    handlers.onClose?.();
  });

  return socket;
};

export const connectTelemetrySocket = (handlers: SocketHandlers<TelemetryIngestedEvent>): WebSocket =>
  connectSocket<TelemetryIngestedEvent>("/ws/telemetry", {
    ...handlers,
    parse: (payload) => {
      const event = parseDashboardEvent(payload);
      if (event == null || !isTelemetryEvent(event)) {
        return null;
      }
      return event;
    }
  });

export const connectAlertSocket = (handlers: SocketHandlers<IngestRejectedEvent>): WebSocket =>
  connectSocket<IngestRejectedEvent>("/ws/alerts", {
    ...handlers,
    parse: (payload) => {
      const event = parseDashboardEvent(payload);
      if (event == null || !isAlertEvent(event)) {
        return null;
      }
      return event;
    }
  });
