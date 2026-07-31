// Collect controlled Ghidra decompilation metrics as JSON.
// @category MIXCOMPILE

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompiledFunction;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class CollectDecompilationMetrics extends GhidraScript {
    private static double seconds(long nanoseconds) {
        return nanoseconds / 1_000_000_000.0;
    }

    private static int lineCount(String text) {
        if (text == null || text.isEmpty()) {
            return 0;
        }
        int lines = 1;
        for (int i = 0; i < text.length(); i++) {
            if (text.charAt(i) == '\n') {
                lines++;
            }
        }
        return lines;
    }

    private static void writeJson(String outputPath, Map<String, Object> result) throws Exception {
        Path path = Paths.get(outputPath);
        Path parent = path.toAbsolutePath().getParent();
        if (parent != null) {
            Files.createDirectories(parent);
        }
        Gson gson = new GsonBuilder().setPrettyPrinting().create();
        try (BufferedWriter writer = Files.newBufferedWriter(path, StandardCharsets.UTF_8)) {
            gson.toJson(result, writer);
            writer.write("\n");
        }
    }

    @Override
    protected void run() throws Exception {
        long scriptStart = System.nanoTime();
        String[] args = getScriptArgs();
        if (args.length < 3) {
            throw new IllegalArgumentException(
                "usage: CollectDecompilationMetrics.java <output.json> <function-timeout-seconds> <mode>");
        }
        String outputPath = args[0];
        int timeoutSeconds = Integer.parseInt(args[1]);
        String mode = args[2];
        Map<String, Object> root = new LinkedHashMap<>();
        root.put("schema_version", 1);
        root.put("mode", mode);
        root.put("java_version", System.getProperty("java.version"));
        root.put("java_user_home", System.getProperty("user.home"));

        if (currentProgram == null) {
            root.put("program_present", false);
            root.put("script_elapsed_seconds", seconds(System.nanoTime() - scriptStart));
            writeJson(outputPath, root);
            return;
        }

        root.put("program_present", true);
        root.put("program_name", currentProgram.getName());
        root.put("executable_path", currentProgram.getExecutablePath());
        root.put("language_id", currentProgram.getLanguageID().toString());
        root.put("compiler_spec_id", currentProgram.getCompilerSpec().getCompilerSpecID().toString());
        root.put("function_timeout_seconds", timeoutSeconds);

        int allFunctions = 0;
        int externalFunctions = 0;
        int thunkFunctions = 0;
        int eligibleFunctions = 0;
        int successfulFunctions = 0;
        int timedOutFunctions = 0;
        int failedFunctions = 0;
        long totalCCharacters = 0;
        long totalCLines = 0;
        long totalHighBasicBlocks = 0;
        List<Map<String, Object>> functionRows = new ArrayList<>();

        DecompInterface decompiler = new DecompInterface();
        try {
            DecompileOptions options = new DecompileOptions();
            decompiler.setOptions(options);
            decompiler.toggleCCode(true);
            decompiler.toggleSyntaxTree(true);
            decompiler.setSimplificationStyle("decompile");
            if (!decompiler.openProgram(currentProgram)) {
                throw new IllegalStateException("decompiler open failed: " + decompiler.getLastMessage());
            }
            long decompileStart = System.nanoTime();
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext()) {
                monitor.checkCancelled();
                Function function = functions.next();
                allFunctions++;
                if (function.isExternal()) {
                    externalFunctions++;
                    continue;
                }
                if (function.isThunk()) {
                    thunkFunctions++;
                    continue;
                }
                eligibleFunctions++;
                Map<String, Object> row = new LinkedHashMap<>();
                row.put("name", function.getName());
                row.put("entry", function.getEntryPoint().toString());
                long functionStart = System.nanoTime();
                DecompileResults result = decompiler.decompileFunction(function, timeoutSeconds, monitor);
                row.put("elapsed_seconds", seconds(System.nanoTime() - functionStart));
                row.put("timed_out", result.isTimedOut());
                row.put("cancelled", result.isCancelled());
                row.put("completed", result.decompileCompleted());
                if (result.decompileCompleted()) {
                    DecompiledFunction decompiled = result.getDecompiledFunction();
                    String c = decompiled == null ? "" : decompiled.getC();
                    int chars = c == null ? 0 : c.length();
                    int lines = lineCount(c);
                    int blocks = result.getHighFunction() == null ? 0 :
                        result.getHighFunction().getBasicBlocks().size();
                    successfulFunctions++;
                    totalCCharacters += chars;
                    totalCLines += lines;
                    totalHighBasicBlocks += blocks;
                    row.put("c_characters", chars);
                    row.put("c_lines", lines);
                    row.put("high_basic_blocks", blocks);
                }
                else {
                    if (result.isTimedOut()) {
                        timedOutFunctions++;
                    }
                    else {
                        failedFunctions++;
                    }
                    row.put("error", result.getErrorMessage());
                }
                functionRows.add(row);
            }
            root.put("decompilation_elapsed_seconds", seconds(System.nanoTime() - decompileStart));
        }
        finally {
            decompiler.closeProgram();
            decompiler.dispose();
        }

        root.put("all_functions", allFunctions);
        root.put("external_functions", externalFunctions);
        root.put("thunk_functions", thunkFunctions);
        root.put("eligible_functions", eligibleFunctions);
        root.put("successful_functions", successfulFunctions);
        root.put("timed_out_functions", timedOutFunctions);
        root.put("failed_functions", failedFunctions);
        root.put("total_c_characters", totalCCharacters);
        root.put("total_c_lines", totalCLines);
        root.put("total_high_basic_blocks", totalHighBasicBlocks);
        root.put("functions", functionRows);
        root.put("script_elapsed_seconds", seconds(System.nanoTime() - scriptStart));
        writeJson(outputPath, root);
    }
}
