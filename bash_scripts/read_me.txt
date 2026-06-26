chronology of script execution
1. setup publisher
2. setup feed handler
    **ping test on both machines
    **eg: on publisher: ping (feed handler's ip)
          on feed handler: ping (publisher's ip)
3. setup dpdk env
4. test
5. teardown dpdk
6. teardown feed handler
7. teardown publisher
