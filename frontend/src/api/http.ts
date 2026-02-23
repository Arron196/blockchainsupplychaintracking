const envBase = import.meta.env.VITE_API_BASE_URL as string | undefined;

export const API_BASE_URL = (envBase ?? "http://127.0.0.1:8080").replace(/\/$/, "");

export class ApiError extends Error {
  public readonly status: number;

  public readonly responseBody: string;

  public constructor(status: number, responseBody: string) {
    super(`API request failed with status ${status}`);
    this.name = "ApiError";
    this.status = status;
    this.responseBody = responseBody;
  }
}

export const toApiUrl = (path: string): string => `${API_BASE_URL}${path}`;

export const fetchJson = async <T>(path: string, init?: RequestInit): Promise<T> => {
  const response = await fetch(toApiUrl(path), {
    method: "GET",
    ...init,
    headers: {
      Accept: "application/json",
      ...(init?.headers ?? {})
    }
  });

  const body = await response.text();
  if (!response.ok) {
    throw new ApiError(response.status, body);
  }

  return JSON.parse(body) as T;
};
