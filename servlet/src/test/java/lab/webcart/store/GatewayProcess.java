package lab.webcart.store;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

/** Starts the C++ webcart-gateway on a free port with a fresh journal. */
public final class GatewayProcess implements AutoCloseable {
    private final Process process;
    private final URI url;
    private final Path journal;

    private GatewayProcess(Process process, URI url, Path journal) {
        this.process = process;
        this.url = url;
        this.journal = journal;
    }

    public static GatewayProcess start(Path binary, Path dataDir) throws IOException {
        Path journal = Files.createTempFile("webcart-", ".journal");
        Process p = new ProcessBuilder(binary.toString(), "--port", "0",
                "--catalog", dataDir.resolve("catalog.csv").toString(),
                "--promotions", dataDir.resolve("promotions.csv").toString(),
                "--journal", journal.toString())
                .redirectError(ProcessBuilder.Redirect.INHERIT)
                .start();
        BufferedReader out = new BufferedReader(new InputStreamReader(p.getInputStream(), StandardCharsets.UTF_8));
        String line = out.readLine();  // "listening on 127.0.0.1:PORT"
        if (line == null || !line.startsWith("listening on ")) {
            p.destroyForcibly();
            throw new IOException("gateway did not start: " + line);
        }
        return new GatewayProcess(p, URI.create("http://" + line.substring("listening on ".length()) + "/webcart/gateway"),
                journal);
    }

    public URI url() {
        return url;
    }

    @Override
    public void close() throws IOException {
        process.destroy();
        try {
            process.waitFor();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        Files.deleteIfExists(journal);
    }
}
