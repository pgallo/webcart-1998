package lab.webcart.store;

import java.nio.file.Path;
import java.time.Duration;
import java.util.stream.Stream;
import org.junit.jupiter.api.Assumptions;
import org.junit.jupiter.api.DynamicTest;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.TestFactory;

class CartLogicTest {
    @TestFactory
    Stream<DynamicTest> cartLogic() {
        return CartLogicChecks.all().stream().map(c -> DynamicTest.dynamicTest(c.name(), c.body()::run));
    }

    @Test
    void httpXmlGatewayAgainstRealGateway() throws Exception {
        String bin = System.getProperty("webcart.gateway.bin", "");
        Assumptions.assumeFalse(bin.isBlank(), "webcart.gateway.bin not set");
        try (GatewayProcess gp = GatewayProcess.start(Path.of(bin), Path.of(System.getProperty("webcart.data.dir")))) {
            GatewayContractCheck.run(new HttpXmlGateway(gp.url(), Duration.ofSeconds(5)));
        }
    }
}
