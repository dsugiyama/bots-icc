#!/usr/bin/env ruby

niter = ARGV[0]
num_threads = [2, 4, 8, 16, 24, 48]

puts 'unit: sec'
puts "# of threads: #{num_threads.join(' ')}"
puts

def run(command, num_iterations)
  iter_cmd = "for i in $(seq #{num_iterations}); do #{command}; done"
  stack_size_kb = 16 * 1024
  result = `ulimit -s #{stack_size_kb}; export OMP_STACKSIZE=#{stack_size_kb}; #{iter_cmd}`
    .scan(/Time Program\s*= ([0-9\.]+)/)
    .flatten
    .map(&:to_f)
    .min
  puts result
end

puts 'serial'
run 'bin/sparselu.icc.serial -n 100 -m 100', niter
puts

puts 'for-untied'
num_threads.each do |n|
  run "OMP_NUM_THREADS=#{n} bin/sparselu.icc.for-omp-tasks -n 100 -m 100", niter
end
puts

puts 'for-tied'
num_threads.each do |n|
  run "OMP_NUM_THREADS=#{n} bin/sparselu.icc.for-omp-tasks-tied -n 100 -m 100", niter
end
puts

puts 'single-untied'
num_threads.each do |n|
  run "OMP_NUM_THREADS=#{n} bin/sparselu.icc.single-omp-tasks -n 100 -m 100", niter
end
puts

puts 'single-tied'
num_threads.each do |n|
  run "OMP_NUM_THREADS=#{n} bin/sparselu.icc.single-omp-tasks-tied -n 100 -m 100", niter
end
